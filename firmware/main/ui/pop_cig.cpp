#include "pop_cig.h"
#include "screensaver.h"
#include "../display/display.h"
#include "../input/input.h"
#include <Arduino.h>
#include <jpeg_decoder.h>
#include "theme.h"
#include "pop_cig_data.h" // MJPEG video for cig
#include "../audio/audio_manager.h"

namespace ui::pop_cig {

static unsigned long s_animStartMs = 0;
static const unsigned long kAnimDurationMs = 3000; // 3 seconds total

static const audio::Note kSmokeAudio[] = {
    { 50, 100 }, { 40, 100 }, { 60, 100 }, { 45, 100 },
    { 55, 100 }, { 35, 100 }, { 65, 100 }, { 40, 100 },
    { 50, 100 }, { 40, 100 }, { 60, 100 }, { 45, 100 },
    { 55, 100 }, { 35, 100 }, { 65, 100 }, { 40, 100 },
    { 50, 100 }, { 40, 100 }, { 60, 100 }, { 45, 100 },
    { 55, 100 }, { 35, 100 }, { 65, 100 }, { 40, 100 },
    { 50, 100 }, { 40, 100 }, { 60, 100 }, { 45, 100 },
    { 55, 100 }, { 35, 100 }
};

static uint16_t* s_decodeBuf = nullptr;
static unsigned int s_mjpegOffset = 0;
static unsigned long s_lastFrameMs = 0;

static void cleanUp() {
    if (s_decodeBuf) {
        free(s_decodeBuf);
        s_decodeBuf = nullptr;
    }
}

void enter() {
    auto& tft = display::tft();
    tft.fillScreen(ST77XX_BLACK);
    s_animStartMs = millis();
    s_lastFrameMs = 0;
    s_mjpegOffset = 0;

    if (!s_decodeBuf) {
        s_decodeBuf = (uint16_t*)malloc(160 * 128 * sizeof(uint16_t));
    }
    
    // Play smoke rumble
    audio::playSfx(kSmokeAudio, sizeof(kSmokeAudio) / sizeof(kSmokeAudio[0]));
}

AppState frame() {
    unsigned long elapsed = millis() - s_animStartMs;
    
    // Playback at ~15fps (66ms per frame)
    if (s_decodeBuf && (millis() - s_lastFrameMs >= 66)) {
        s_lastFrameMs = millis();
        
        unsigned int start = s_mjpegOffset;
        while (start < cig_mjpeg_len - 1 && !(cig_mjpeg[start] == 0xFF && cig_mjpeg[start+1] == 0xD8)) {
            start++;
        }
        
        if (start >= cig_mjpeg_len - 1 || elapsed > kAnimDurationMs) {
            // Reached end of video or duration
            cleanUp();
            screensaver::activateSpeedBoost();
            return AppState::Screensaver;
        }
        
        unsigned int end = start;
        while (end < cig_mjpeg_len - 1 && !(cig_mjpeg[end] == 0xFF && cig_mjpeg[end+1] == 0xD9)) {
            end++;
        }
        
        if (end < cig_mjpeg_len - 1) {
            end += 2; // Include FF D9
            
            esp_jpeg_image_cfg_t jpegCfg = {};
            jpegCfg.indata = const_cast<uint8_t*>(&cig_mjpeg[start]);
            jpegCfg.indata_size = end - start;
            jpegCfg.outbuf = reinterpret_cast<uint8_t*>(s_decodeBuf);
            jpegCfg.outbuf_size = 160 * 128 * 2;
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
                        uint16_t c = s_decodeBuf[y * out.width + x];
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

    if (input::wasPressed(input::Button::Back) || input::wasPressed(input::Button::Ok)) {
        cleanUp();
        screensaver::activateSpeedBoost();
        return AppState::Screensaver;
    }

    return AppState::PopCig;
}

} // namespace ui::pop_cig
