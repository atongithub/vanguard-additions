#include "8ball.h"
#include "../display/display.h"
#include "../input/input.h"
#include "8ball_video_data.h"
#include "theme.h"
#include <Arduino.h>
#include <esp_random.h>
#include <jpeg_decoder.h>
#include <cstdlib>

namespace ui::eightball {

static bool s_thinking = false;
static const char* s_answer = nullptr;
static uint16_t* s_animBuf = nullptr;
static unsigned long s_lastFrameMs = 0;
static unsigned int s_mjpegOffset = 0;

static const char* kAnswers[] = {
    "It is certain.",
    "It is decidedly so.",
    "Without a doubt gng.",
    "Yes definitely.",
    "You may rely on it.",
    "As I see it, yes.",
    "Most likely.",
    "Outlook good.",
    "Yes.",
    "Signs point to yes.",
    "Reply hazy, try again.",
    "Ask again later.",
    "Better not tell you now.",
    "Cannot predict now, mb gng.",
    "Concentrate and ask again.",
    "Don't count on it.",
    "My reply is no.",
    "My sources say no.",
    "Outlook not so good.",
    "Very doubtful.",
    "bruh no.",
    "skill issue.",
    "touch grass.",
    "I miss my wife.",
    "get a life instead lol.",
    "just dont.",
    "look to your left.",
    "actually ykw? nvm.",
    "cope",
    "I was gonna say something.",
    "really? thats your question?"
};
static constexpr int kNumAnswers = sizeof(kAnswers) / sizeof(kAnswers[0]);

static void drawNextFrame(bool advance) {
    if (!s_animBuf) return;
    
    unsigned int start = s_mjpegOffset;
    while (start < eightball_mjpeg_len - 1 && !(eightball_mjpeg[start] == 0xFF && eightball_mjpeg[start+1] == 0xD8)) {
        start++;
    }
    
    if (start >= eightball_mjpeg_len - 1) {
        if (!advance) return;
        // Loop back to beginning
        s_mjpegOffset = 0;
        start = 0;
        while (start < eightball_mjpeg_len - 1 && !(eightball_mjpeg[start] == 0xFF && eightball_mjpeg[start+1] == 0xD8)) {
            start++;
        }
        if (start >= eightball_mjpeg_len - 1) return;
    }
    
    unsigned int end = start;
    while (end < eightball_mjpeg_len - 1 && !(eightball_mjpeg[end] == 0xFF && eightball_mjpeg[end+1] == 0xD9)) {
        end++;
    }
    
    if (end < eightball_mjpeg_len - 1) {
        end += 2;
        
        esp_jpeg_image_cfg_t jpegCfg = {};
        jpegCfg.indata = const_cast<uint8_t*>(&eightball_mjpeg[start]);
        jpegCfg.indata_size = end - start;
        jpegCfg.outbuf = reinterpret_cast<uint8_t*>(s_animBuf);
        jpegCfg.outbuf_size = 160 * 120 * 2;
        jpegCfg.out_format = JPEG_IMAGE_FORMAT_RGB565;
        jpegCfg.out_scale = JPEG_IMAGE_SCALE_0;
        jpegCfg.flags.swap_color_bytes = 0;

        esp_jpeg_image_output_t out = {};
        esp_err_t err = esp_jpeg_decode(&jpegCfg, &out);
        
        if (err == ESP_OK) {
            auto& tft = display::tft();
            // Scale 160x120 -> 320x240
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
        
        if (advance) {
            s_mjpegOffset = end;
        }
    }
}

void enter() {
    s_thinking = false;
    s_answer = nullptr;
    s_mjpegOffset = 0;
    s_lastFrameMs = 0;
    
    if (!s_animBuf) {
        s_animBuf = (uint16_t*)malloc(160 * 120 * sizeof(uint16_t));
    }
    
    auto& tft = display::tft();
    tft.fillScreen(ST77XX_BLACK);
    
    drawNextFrame(false);
    theme::drawCentered(tft, "Hold OK to shake", 220, theme::BODY_TEXT_SIZE, theme::COLOR_TEXT);
}

AppState frame() {
    auto& tft = display::tft();

    if (input::wasPressed(input::Button::Back)) {
        if (s_animBuf) {
            free(s_animBuf);
            s_animBuf = nullptr;
        }
        return AppState::MainMenu;
    }

    bool isOkDown = input::isDown(input::Button::Ok) || input::isDown(input::Button::JoySelect);

    if (isOkDown) {
        if (!s_thinking) {
            s_thinking = true;
            s_answer = nullptr;
            tft.fillScreen(ST77XX_BLACK);
        }
        
        if (millis() - s_lastFrameMs >= 66) {
            s_lastFrameMs = millis();
            drawNextFrame(true);
        }
    } else {
        if (s_thinking) {
            s_thinking = false;
            s_answer = kAnswers[esp_random() % kNumAnswers];
            
            s_mjpegOffset = 0;
            drawNextFrame(false);
            tft.fillRect(0, 100, 320, 50, ST77XX_BLACK);
            // Bigger text in blue
            theme::drawCentered(tft, s_answer, 120, 2, ST77XX_BLUE);
        }
    }

    return AppState::EightBall;
}

} // namespace ui::eightball
