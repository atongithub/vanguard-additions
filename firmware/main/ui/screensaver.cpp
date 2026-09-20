#include "screensaver.h"
#include "theme.h"
#include "../../include/config.h"
#include "../display/display.h"
#include "../input/input.h"
#include "../leds/led_manager.h"
#include <Arduino.h>
#include <jpeg_decoder.h>
#include "video_data.h"

namespace ui::screensaver {

static uint16_t* s_decodeBuf = nullptr;
static unsigned int s_mjpegOffset = 0;
static unsigned long s_lastFrameMs = 0;
static unsigned long s_speedBoostEndMs = 0;
static bool s_wasSpeedBoostActive = false;

void activateSpeedBoost() {
    s_speedBoostEndMs = millis() + 30000;
}

void enter() {
    auto& tft = display::tft();
    tft.fillScreen(ST77XX_BLACK);
    s_wasSpeedBoostActive = millis() < s_speedBoostEndMs;
    uint16_t period = s_wasSpeedBoostActive ? 30 : 0; // 0 means default (80ms)
    led::playEffect(led::EffectId::Sweep, led::EffectParams{0, 0, period, (uint16_t)(led::kMaskRed | led::kMaskGreen)});
    
    if (!s_decodeBuf) {
        // Allocate 160x120 RGB565 buffer (38,400 bytes)
        s_decodeBuf = (uint16_t*)malloc(160 * 120 * sizeof(uint16_t));
    }
    s_mjpegOffset = 0;
    s_lastFrameMs = millis();
}

static bool anyNavInput() {
    return input::wasPressed(input::Button::JoyUp) ||
           input::wasPressed(input::Button::JoyDown) ||
           input::wasPressed(input::Button::JoyLeft) ||
           input::wasPressed(input::Button::JoyRight) ||
           input::wasPressed(input::Button::JoySelect);
}

AppState frame() {
    if (anyNavInput()) {
        led::stop();
        if (s_decodeBuf) { free(s_decodeBuf); s_decodeBuf = nullptr; }
        return AppState::MainMenu;
    }
    if (input::wasPressed(input::Button::Ok)) {
        led::stop();
        if (s_decodeBuf) { free(s_decodeBuf); s_decodeBuf = nullptr; }
        return AppState::ProfileSetup;
    }

    // Playback at ~15fps (66ms per frame), or ~30fps (33ms) if speed boost is active
    bool speedBoostActive = millis() < s_speedBoostEndMs;
    if (speedBoostActive != s_wasSpeedBoostActive) {
        s_wasSpeedBoostActive = speedBoostActive;
        uint16_t period = s_wasSpeedBoostActive ? 30 : 0;
        led::playEffect(led::EffectId::Sweep, led::EffectParams{0, 0, period, (uint16_t)(led::kMaskRed | led::kMaskGreen)});
    }
    
    unsigned long frameInterval = speedBoostActive ? 33 : 66;

    if (s_decodeBuf && (millis() - s_lastFrameMs >= frameInterval)) {
        s_lastFrameMs = millis();
        
        // Find next JPEG frame
        unsigned int start = s_mjpegOffset;
        while (start < video_mjpeg_len - 1 && !(video_mjpeg[start] == 0xFF && video_mjpeg[start+1] == 0xD8)) {
            start++;
        }
        
        if (start >= video_mjpeg_len - 1) {
            // Reached end of video, loop back to start
            s_mjpegOffset = 0;
            return AppState::Screensaver;
        }
        
        unsigned int end = start;
        while (end < video_mjpeg_len - 1 && !(video_mjpeg[end] == 0xFF && video_mjpeg[end+1] == 0xD9)) {
            end++;
        }
        
        if (end < video_mjpeg_len - 1) {
            end += 2; // Include FF D9
            
            // Decode the JPEG frame
            esp_jpeg_image_cfg_t jpegCfg = {};
            jpegCfg.indata = const_cast<uint8_t*>(&video_mjpeg[start]);
            jpegCfg.indata_size = end - start;
            jpegCfg.outbuf = reinterpret_cast<uint8_t*>(s_decodeBuf);
            jpegCfg.outbuf_size = 160 * 120 * 2;
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
        } else {
            s_mjpegOffset = 0; // End of file
        }
    }

    return AppState::Screensaver;
}

} // namespace ui::screensaver
