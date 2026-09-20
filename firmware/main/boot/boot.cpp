#include "boot.h"
#include "../../include/config.h"
#include "../display/display.h"
#include "../display/logo_data.h"
#include "../leds/led_manager.h"
#include "../audio/audio_manager.h"
#include <Arduino.h>
#include <cstdio>
#include <jpeg_decoder.h>
#include "boot_video_data.h" // MJPEG video for boot
#include "boot_audio_data.h" // Converted audio array

namespace boot {

enum class Phase { Logo };
static Phase s_phase = Phase::Logo;
static unsigned long s_phaseStartMs = 0;
static unsigned long s_lastFrameMs = 0;
static unsigned int s_mjpegOffset = 0;

// (old kMeow removed)

static uint16_t* s_animBuf = nullptr;
static bool s_meowFired = false;

static void leaveLogoAnim() {
    if (s_animBuf) {
        free(s_animBuf);
        s_animBuf = nullptr;
    }
}

static void enterLogo() {
    s_phase = Phase::Logo;
    s_phaseStartMs = millis();
    s_lastFrameMs = 0;
    s_mjpegOffset = 0;
    s_meowFired = false;
    
    // Allocate 160x128 RGB565 buffer (padded for JPEG MCU)
    s_animBuf = (uint16_t*)malloc(160 * 128 * sizeof(uint16_t));
    
    // Start LED sweep
    led::playEffect(led::EffectId::BootSweep);
}

void enter() {
    enterLogo();
}

AppState frame() {
    unsigned long elapsed = millis() - s_phaseStartMs;
    
    if (s_phase == Phase::Logo) {
        // Play audio earlier or wait until video finishes, but here we play at 0s
        if (!s_meowFired) {
            s_meowFired = true;
            audio::playMelody(kBootMelody, kBootMelodyCount, /*loop=*/false);
        }

        // We can just end the boot sequence at 7000ms like before, 
        // OR when the video ends. Let's say we end when the video ends or at 7s, whichever is later.
        // But for safety, we'll just end when the video ends.
        
        // Playback at ~15fps (66ms per frame)
        if (s_animBuf && (millis() - s_lastFrameMs >= 66)) {
            s_lastFrameMs = millis();
            
            unsigned int start = s_mjpegOffset;
            while (start < video_mjpeg_len - 1 && !(video_mjpeg[start] == 0xFF && video_mjpeg[start+1] == 0xD8)) {
                start++;
            }
            
            if (start >= video_mjpeg_len - 1) {
                // Reached end of video!
                leaveLogoAnim();
                led::stop();
                return AppState::Screensaver;
            }
            
            unsigned int end = start;
            while (end < video_mjpeg_len - 1 && !(video_mjpeg[end] == 0xFF && video_mjpeg[end+1] == 0xD9)) {
                end++;
            }
            
            if (end < video_mjpeg_len - 1) {
                end += 2; // Include FF D9
                
                esp_jpeg_image_cfg_t jpegCfg = {};
                jpegCfg.indata = const_cast<uint8_t*>(&video_mjpeg[start]);
                jpegCfg.indata_size = end - start;
                jpegCfg.outbuf = reinterpret_cast<uint8_t*>(s_animBuf);
                jpegCfg.outbuf_size = 160 * 128 * 2; // Pad height to 128 (multiple of 16 for JPEG MCU)
                jpegCfg.out_format = JPEG_IMAGE_FORMAT_RGB565;
                jpegCfg.out_scale = JPEG_IMAGE_SCALE_0;
                jpegCfg.flags.swap_color_bytes = 0;

                esp_jpeg_image_output_t out = {};
                esp_err_t err = esp_jpeg_decode(&jpegCfg, &out);
                
                if (err == ESP_OK) {
                    auto& tft = display::tft();
                    uint16_t line[320];
                    for (int y = 0; y < out.height; y++) {
                        for (int x = 0; x < out.width; x++) {
                            uint16_t c = s_animBuf[y * out.width + x];
                            line[x * 2] = c;
                            line[x * 2 + 1] = c;
                        }
                        tft.drawRGBBitmap(0, y * 2, line, 320, 1);
                        tft.drawRGBBitmap(0, y * 2 + 1, line, 320, 1);
                    }
                }
                
                s_mjpegOffset = end;
            }
        }
        
        // Timeout if it gets stuck for some reason (e.g. video is too long or decoding fails)
        if (elapsed > 15000) {
            leaveLogoAnim();
            led::stop();
            return AppState::Screensaver;
        }

        return AppState::Boot;
    }
    
    return AppState::Screensaver;
}

} // namespace boot
