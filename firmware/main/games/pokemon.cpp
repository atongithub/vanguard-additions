#include "pokemon.h"
#include "../display/display.h"
#include "../input/input.h"
#include "../../include/config.h"
#include "esp_partition.h"
#include "esp_spi_flash.h"
#include <Arduino.h>

#define ENABLE_SOUND 0
#define PEANUT_GB_HIGH_LCD_ACCURACY 0
#include "peanut_gb.h"

namespace games::pokemon {

static gb_s s_gb;
static spi_flash_mmap_handle_t s_mmap_handle;
static const uint8_t* s_rom_ptr = nullptr;
static uint8_t s_ram[32768]; // 32KB is enough for Pokemon Red/Blue

uint8_t gb_rom_read(struct gb_s *gb, const uint_fast32_t addr) {
    return s_rom_ptr[addr];
}

uint8_t gb_cart_ram_read(struct gb_s *gb, const uint_fast32_t addr) {
    return s_ram[addr];
}

void gb_cart_ram_write(struct gb_s *gb, const uint_fast32_t addr, const uint8_t val) {
    s_ram[addr] = val;
}

void gb_error(struct gb_s *gb, const enum gb_error_e gb_err, const uint16_t val) {
    // Ignore errors for now
}

void lcd_draw_line(struct gb_s *gb, const uint8_t pixels[160], const uint_fast8_t line) {
    // Peanut-GB palette: 0=White, 1=Light Gray, 2=Dark Gray, 3=Black
    // RGB565 format
    static const uint16_t palette[4] = {
        0xFFFF, // White
        0xAD55, // Light Gray
        0x52AA, // Dark Gray
        0x0000  // Black
    };

    if (line >= 12 && line < 132) {
        static uint16_t line_buffer[640];
        for(int x = 0; x < 160; x++) {
            uint16_t color = palette[pixels[x] & 3];
            line_buffer[x * 2] = color;
            line_buffer[x * 2 + 1] = color;
            line_buffer[320 + x * 2] = color;
            line_buffer[320 + x * 2 + 1] = color;
        }
        auto& tft = display::tft();
        int screen_y = (line - 12) * 2;
        tft.drawRGBBitmap(0, screen_y, line_buffer, 320, 2);
    }
}

void enter() {
    auto& tft = display::tft();
    tft.fillScreen(ST77XX_BLACK);

    if (s_rom_ptr == nullptr) {
        const esp_partition_t* part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, "gb_rom");
        if (part != nullptr) {
            esp_partition_mmap(part, 0, part->size, ESP_PARTITION_MMAP_DATA, (const void**)&s_rom_ptr, &s_mmap_handle);
        }
    }

    if (s_rom_ptr == nullptr) {
        tft.setTextColor(ST77XX_WHITE);
        tft.setTextSize(1);
        tft.setCursor(10, 60);
        tft.print("ROM not found!");
        return;
    }

    gb_init(&s_gb, &gb_rom_read, &gb_cart_ram_read, &gb_cart_ram_write, &gb_error, nullptr);
    gb_init_lcd(&s_gb, &lcd_draw_line);
}

AppState frame() {
    vTaskDelay(pdMS_TO_TICKS(1));
    if (input::wasPressed(input::Button::Pause)) {
        return AppState::GamesMenu;
    }

    if (s_rom_ptr == nullptr) {
        if (input::wasPressed(input::Button::Back)) return AppState::GamesMenu;
        return AppState::Pokemon;
    }

    // Map inputs
    s_gb.direct.joypad = 0xFF; // All unpressed
    if (input::isDown(input::Button::JoyUp)) s_gb.direct.joypad &= ~JOYPAD_UP;
    if (input::isDown(input::Button::JoyDown)) s_gb.direct.joypad &= ~JOYPAD_DOWN;
    if (input::isDown(input::Button::JoyLeft)) s_gb.direct.joypad &= ~JOYPAD_LEFT;
    if (input::isDown(input::Button::JoyRight)) s_gb.direct.joypad &= ~JOYPAD_RIGHT;
    if (input::isDown(input::Button::Ok) || input::isDown(input::Button::JoySelect)) s_gb.direct.joypad &= ~JOYPAD_A;
    if (input::isDown(input::Button::Back)) s_gb.direct.joypad &= ~JOYPAD_B;
    if (input::isDown(input::Button::Start)) s_gb.direct.joypad &= ~JOYPAD_START;

    gb_run_frame(&s_gb);

    return AppState::Pokemon;
}

} // namespace games::pokemon
