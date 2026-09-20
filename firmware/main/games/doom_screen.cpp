#include "doom_screen.h"
#include "../input/input.h"
#include "../display/display.h"
#include "../audio/audio_manager.h"
#include "../leds/led_manager.h"
#include "../rf/lora.h"
#include "../settings/settings.h"
#include <Arduino.h>
#include "driver/uart.h"
#include "driver/gpio.h"

namespace games::doom {

static bool s_doomInitialized = false;

void enter() {
    // Shut down background radio, LEDs, and WiFi/BLE to free as much RAM as possible
    led::stop();
    rf::stopReceiving();
    settings::stopWifiPortalIfActive();

    if (!s_doomInitialized) {
        Adafruit_ST7789& tft = display::tft();
        tft.fillScreen(ST77XX_BLACK);
        tft.setCursor(10, 110);
        tft.setTextColor(ST77XX_WHITE);
        tft.setTextSize(2);
        tft.print("AWAITING SIGNAL...");
        
        // Initialize UART1 for high-speed terminal link
        uart_config_t uart_config = {
            .baud_rate = 5000000,
            .data_bits = UART_DATA_8_BITS,
            .parity    = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .source_clk = UART_SCLK_DEFAULT,
        };
        
        // Install driver with 4096 byte RX buffer and 256 byte TX buffer
        // The RX buffer only needs to absorb minor task scheduling jitter, as we read the frame directly into PSRAM.
        esp_err_t err = uart_driver_install(UART_NUM_1, 4096, 256, 0, NULL, 0);
        if (err != ESP_OK) {
            printf("Failed to install UART driver: %d\n", err);
        }
        uart_param_config(UART_NUM_1, &uart_config);
        uart_set_pin(UART_NUM_1, 43, 44, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

        s_doomInitialized = true;
    }
}
AppState frame() {
    // 1. Pack buttons into a single byte mask
    uint8_t btnMask = 0;
    if (input::isDown(input::Button::JoyUp))    btnMask |= (1 << 0);
    if (input::isDown(input::Button::JoyDown))  btnMask |= (1 << 1);
    if (input::isDown(input::Button::JoyLeft))  btnMask |= (1 << 2);
    if (input::isDown(input::Button::JoyRight)) btnMask |= (1 << 3);
    if (input::isDown(input::Button::Pause))    btnMask |= (1 << 4);
    if (input::isDown(input::Button::Ok))       btnMask |= (1 << 5);
    if (input::isDown(input::Button::Back))     btnMask |= (1 << 6);
    if (input::isDown(input::Button::Start))    btnMask |= (1 << 7);

    // Send button state to N16R8 engine
    uart_write_bytes(UART_NUM_1, &btnMask, 1);

    // 2. Poll for incoming frames from the N16R8
    // We expect a magic header "DOOM" (4 bytes) + 38400 bytes of RGB565 (160x120)
    const int FRAME_SIZE = 38400; 

    size_t length = 0;
    uart_get_buffered_data_len(UART_NUM_1, &length);

    // Look for magic header first
    while (length >= 1) {
        uint8_t b;
        uart_read_bytes(UART_NUM_1, &b, 1, 0); // Read 1 byte
        if (b == 'D') {
            uint8_t rest[3];
            int read = uart_read_bytes(UART_NUM_1, rest, 3, 50 / portTICK_PERIOD_MS);
            if (read == 3 && rest[0] == 'O' && rest[1] == 'O' && rest[2] == 'M') {
                // FOUND HEADER! Now block and wait for the rest of the frame
                uint8_t* rxBuf = (uint8_t*)malloc(FRAME_SIZE);
                if (rxBuf) {
                    // It takes ~76ms to transmit 38400 bytes at 5Mbps. Wait up to 500ms.
                    int frame_read = uart_read_bytes(UART_NUM_1, rxBuf, FRAME_SIZE, 500 / portTICK_PERIOD_MS);
                    if (frame_read == FRAME_SIZE) {
                        Adafruit_ST7789& tft = display::tft();
                        uint16_t* pixels = (uint16_t*)rxBuf;
                        uint16_t line[320];
                        for (int y = 0; y < 120; y++) {
                            for (int x = 0; x < 160; x++) {
                                uint16_t c = pixels[y * 160 + x];
                                line[x * 2] = c;
                                line[x * 2 + 1] = c;
                            }
                            tft.drawRGBBitmap(0, y * 2, line, 320, 1);
                            tft.drawRGBBitmap(0, y * 2 + 1, line, 320, 1);
                        }
                    }
                    free(rxBuf);
                }
                break; // Frame drawn or dropped, exit loop for this tick
            }
        }
        uart_get_buffered_data_len(UART_NUM_1, &length);
    }

    // Give an emergency exit hatch in case the user wants to leave (Start + Back)
    if (input::isDown(input::Button::Start) && input::wasPressed(input::Button::Back)) {
        uart_driver_delete(UART_NUM_1);
        s_doomInitialized = false;
        return AppState::GamesMenu;
    }

    return AppState::Doom;
}

} // namespace games::doom
