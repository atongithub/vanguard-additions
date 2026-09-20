#include "menu.h"
#include "renderer.h"
#include "widgets.h"
#include "theme.h"
#include "../../include/config.h"
#include "../display/display.h"
#include "../input/input.h"
#include "../power/battery.h"
#include "../leds/led_manager.h"
#include "../audio/audio_manager.h"
#include "../display/logo_data.h"
#include "../display/vanguard_logo_data.h"
#include "starfield.h"
#include "animation.h"
#include <Arduino.h>
#include <cstdio>

namespace ui {

// Lowered from 20: fired too early/often to mean much. 10% is a genuine "plug this in soon" threshold.
constexpr uint8_t kLowBatteryPercent = 10;
// Hard floor ignoring power::isCharging(): the charging heuristic can misread a flat discharge
// plateau as charging, which would otherwise hide the warning at genuinely critical charge.
constexpr uint8_t kCriticalBatteryPercent = 5;

// ---- Main menu ----
static const char* kMainItems[] = {
    "Challenges", "Games", "Music Player", "Pop a Cig", "Magic 8 Ball",
    "Settings & Diagnostics", "Tsundere Miku GF", "Paarivendhar GF"
};
static const int kMainCount = 8;

static int s_mainSelected = 0;
// bit0 = low-battery active, bit1 = blink phase — packed together so a phase toggle alone still
// counts as a state change for the Region's dirty-check.
static uint8_t s_lowBattValue = 0;
static bool s_battWasLow = false;
static bool s_battBlinkOn = false;
static anim::AnimId s_battBlinkAnim = anim::kInvalidAnim;

static void onBattBlinkToggle(bool on, void* /*ctx*/) {
    s_battBlinkOn = on;
}

// ---- Secret button combo ----
// Konami-code-shaped sequence (Back/Ok standing in for B/A), tracked as a side-channel that only
// watches normal nav presses rather than intercepting them. Pure easter egg, no gameplay effect.
static const input::Button kSecretSequence[] = {
    input::Button::JoyUp, input::Button::JoyUp,
    input::Button::JoyDown, input::Button::JoyDown,
    input::Button::JoyLeft, input::Button::JoyRight,
    input::Button::JoyLeft, input::Button::JoyRight,
    input::Button::Back, input::Button::Ok,
};
constexpr int kSecretSequenceLen = sizeof(kSecretSequence) / sizeof(kSecretSequence[0]);
static int s_secretProgress = 0;

static const audio::Note kSecretJingle[] = {
    { 1046, 70 }, { 1318, 70 }, { 1568, 70 }, { 2093, 70 },
    { 1568, 70 }, { 2093, 160 },
};
constexpr uint16_t kSecretJingleCount = sizeof(kSecretJingle) / sizeof(kSecretJingle[0]);
constexpr unsigned long kSecretToastMs = 2500;

static bool s_secretToastActive = false;
static unsigned long s_secretToastUntil = 0;

// Returns true exactly on the press that completes the whole sequence.
static bool feedSecretSequence(input::Button b) {
    if (b == kSecretSequence[s_secretProgress]) {
        s_secretProgress++;
        if (s_secretProgress >= kSecretSequenceLen) {
            s_secretProgress = 0;
            return true;
        }
        return false;
    }
    // Mismatch: restart, but count this press as a fresh attempt if it's also the sequence's
    // first button (so "UP UP UP DOWN DOWN..." still finds the code inside the extra UP).
    s_secretProgress = (b == kSecretSequence[0]) ? 1 : 0;
    return false;
}

static void drawSecretToast() {
    auto& tft = display::tft();
    ui::Rect box{20, 90, (int16_t)(cfg::DISPLAY_WIDTH - 40), 60};
    ui::widgets::popup(tft, box, "VAJRA MODE UNLOCKED", theme::COLOR_HIGHLIGHT,
                       theme::COLOR_HIGHLIGHT_TEXT);
}

// One row = one Region, packed state = row index | (selected<<7). Per-row Regions fix the
// "visible flash when moving up/down" — a single whole-list Region would redraw all rows on
// every keypress instead of just the (at most 2) rows whose selection actually flipped.
static void drawMainRow(Adafruit_ST7789& tft, Rect bounds, const void* state) {
    uint8_t v = *(const uint8_t*)state;
    int idx = v & 0x7F;
    bool selected = v & 0x80;
    widgets::listItem(tft, bounds, idx, kMainItems[idx], selected);
}

static void drawStatus(Adafruit_ST7789& tft, Rect bounds, const void* state) {
    uint8_t v = *(const uint8_t*)state;
    if (!(v & 0x01)) return; // nothing drawn when battery isn't low — Renderer already cleared the region
    bool blinkOn = v & 0x02;
    if (!blinkOn) return; // blink "off" phase — leave the region blank, same as not-low
    tft.setTextColor(theme::COLOR_DANGER);
    tft.setTextSize(theme::BODY_TEXT_SIZE);
    tft.setCursor(bounds.x, bounds.y);
    tft.print("LOW BATT");
}

UI_ASSERT_REGION_STATE_FITS(uint8_t); // covers both s_mainRowState and s_lowBattValue

static uint8_t s_mainRowState[kMainCount];
static Region s_mainRegions[kMainCount + 1];
static bool s_mainRegionsInit = false;
constexpr int16_t kMainStartY = 38;
constexpr int16_t kMainItemHeight = 22;

static Region* mainMenuRegions(int* count) {
    if (!s_mainRegionsInit) {
        for (int i = 0; i < kMainCount; i++) {
            s_mainRegions[i] = Region{
                Rect{0, (int16_t)(kMainStartY + i * kMainItemHeight),
                     (int16_t)cfg::DISPLAY_WIDTH, (int16_t)kMainItemHeight},
                drawMainRow, nullptr, &s_mainRowState[i], sizeof(uint8_t), theme::COLOR_BG, {}, false};
        }
        s_mainRegions[kMainCount] = Region{
            Rect{(int16_t)(cfg::DISPLAY_WIDTH - 70), (int16_t)theme::HEADER_Y, 70, 16},
            drawStatus, nullptr, &s_lowBattValue, sizeof(uint8_t), theme::COLOR_BG, {}, false};
        s_mainRegionsInit = true;
    }
    for (int i = 0; i < kMainCount; i++) {
        s_mainRowState[i] = (uint8_t)i | (i == s_mainSelected ? 0x80 : 0);
    }
    *count = kMainCount + 1;
    return s_mainRegions;
}

// The strip below the list rows is the one part of this screen no Region ever reaches — a
// starfield drawn across the whole menu would visibly empty out as row redraws erase stars
// during navigation. Only this belt animates, since nothing redraws over it.
constexpr int16_t kMenuBeltY = (int16_t)(kMainStartY + kMainCount * kMainItemHeight) + 2;
constexpr int16_t kMenuBeltH = (int16_t)(cfg::DISPLAY_HEIGHT - kMenuBeltY);
static unsigned long s_menuBeltTwinkleAtMs = 0;

// Paints the starfield backdrop on Screen::onEnter, called after clearScreen() and before
// header/regions draw. Starfield sky colour is theme::COLOR_BG, same as what the rows repaint
// themselves in, so a redundant onEnter() (via invalidate()) reads as a no-op, not a flash.
static void drawMainMenuStarfield() {
    // Not starfield::drawFull(): it scatters stars across the whole list area, which rows would
    // immediately erase on redraw. Only the belt (never touched by a Region) is drawn.
    ui::starfield::drawBelt(display::tft(), 0, kMenuBeltY, (int16_t)cfg::DISPLAY_WIDTH, kMenuBeltH);
}

static Screen kMainMenuScreen{drawMainMenuStarfield, mainMenuRegions, "Main Menu"};

// Stops the low-battery blink/LED warning and resets the edge-detect flag so the next
// low-battery observation is treated as a fresh transition. Called on leaving Main Menu (so
// another screen doesn't inherit the LED effect) and defensively on entry.
static void stopLowBatteryWarning() {
    if (s_battBlinkAnim != anim::kInvalidAnim) {
        anim::cancel(s_battBlinkAnim);
        s_battBlinkAnim = anim::kInvalidAnim;
    }
    if (s_battWasLow) led::stop();
    s_battWasLow = false;
    s_battBlinkOn = false;
}

void mainMenuEnter() {
    s_mainSelected = 0;
    stopLowBatteryWarning();
    invalidate(); // required so re-entry after an unmigrated screen forces a full redraw, not a diff
}

AppState mainMenuFrame() {
    if (s_secretToastActive) {
        // Frozen: toast is drawn directly over the menu (not through a Region), so letting
        // frame() run underneath would redraw menu pieces back over it.
        if (millis() >= s_secretToastUntil) {
            s_secretToastActive = false;
            invalidate(); // wipe the toast by forcing a full menu redraw
        }
        return AppState::MainMenu;
    }

    // Safe to tick every frame: confined to the belt strip nothing else on this screen draws to.
    if (millis() - s_menuBeltTwinkleAtMs >= 200) {
        s_menuBeltTwinkleAtMs = millis();
        starfield::twinkleBelt(display::tft(), 0, kMenuBeltY, (int16_t)cfg::DISPLAY_WIDTH, kMenuBeltH);
    }

    if (input::wasPressed(input::Button::JoyUp)) {
        s_mainSelected = (s_mainSelected - 1 + kMainCount) % kMainCount;
        widgets::navSfx();
        feedSecretSequence(input::Button::JoyUp);
    }
    if (input::wasPressed(input::Button::JoyDown)) {
        s_mainSelected = (s_mainSelected + 1) % kMainCount;
        widgets::navSfx();
        feedSecretSequence(input::Button::JoyDown);
    }
    // Left/Right have no other function here — they exist purely to feed the secret tracker.
    if (input::wasPressed(input::Button::JoyLeft))  feedSecretSequence(input::Button::JoyLeft);
    if (input::wasPressed(input::Button::JoyRight)) feedSecretSequence(input::Button::JoyRight);
    bool backPressed = input::wasPressed(input::Button::Back);
    // Back is also the secret sequence's 9th input; if continuing an in-progress sequence,
    // consume it for that instead of navigating away.
    bool backIsSecretStep = backPressed && kSecretSequence[s_secretProgress] == input::Button::Back;
    if (backPressed) feedSecretSequence(input::Button::Back);
    if (backPressed && !backIsSecretStep) {
        stopLowBatteryWarning();
        return AppState::Screensaver;
    }

    uint8_t battPct = power::batteryPercent();
    bool low = battPct < kCriticalBatteryPercent ||
               (battPct < kLowBatteryPercent && !power::isCharging());
    if (low && !s_battWasLow) {
        // Just crossed into low-battery: start text blink + red LED pulse together so the
        // warning is hard to miss on screen or LEDs.
        s_battBlinkAnim = anim::startBlink(500, onBattBlinkToggle, nullptr);
        led::playEffect(led::EffectId::Warning, led::EffectParams{0, 0, 0, led::kMaskRed});
    } else if (!low && s_battWasLow) {
        stopLowBatteryWarning();
    }
    s_battWasLow = low;

    s_lowBattValue = (low ? 0x01 : 0) | (s_battBlinkOn ? 0x02 : 0);

    frame(kMainMenuScreen);

    // Ok is both "confirm" and the secret sequence's final button; the completing press must
    // be consumed by the easter egg, not also navigate away the same tick.
    bool okPressed = input::wasPressed(input::Button::Ok);
    bool secretCompleted = okPressed && feedSecretSequence(input::Button::Ok);
    if (secretCompleted) {
        s_secretToastActive = true;
        s_secretToastUntil = millis() + kSecretToastMs;
        led::playEffect(led::EffectId::BootSweep);
        audio::playSfx(kSecretJingle, kSecretJingleCount);
        drawSecretToast();
        return AppState::MainMenu;
    }

    bool confirm = (okPressed && !secretCompleted) ||
                   input::wasPressed(input::Button::JoySelect);
    if (confirm) {
        stopLowBatteryWarning(); // leaving the screen this warning is scoped to
        switch (s_mainSelected) {
            case 0: return AppState::Challenges;
            case 1: return AppState::GamesMenu;
            case 2: return AppState::MusicPlayer;
            case 3: return AppState::PopCig;
            case 4: return AppState::EightBall;
            case 5: return AppState::Settings;
            case 6: return AppState::GirlfriendMiku;
            case 7: return AppState::GirlfriendPaari;
        }
    }
    return AppState::MainMenu;
}

// ---- Games submenu ("Game Session") ----
static const char* kGamesItems[] = { "Tetris", "Snake", "Space Shooter", "2048", "Doom", "Pokemon" };
static const int kGamesCount = 6;
static int s_gamesSelected = 0;

static void drawGamesRow(Adafruit_ST7789& tft, Rect bounds, const void* state) {
    uint8_t v = *(const uint8_t*)state;
    int idx = v & 0x7F;
    bool selected = v & 0x80;
    widgets::listItem(tft, bounds, idx, kGamesItems[idx], selected);
}

UI_ASSERT_REGION_STATE_FITS(uint8_t); // s_gamesRowState

static uint8_t s_gamesRowState[kGamesCount];
static Region s_gamesRegions[kGamesCount];
static bool s_gamesRegionsInit = false;

static Region* gamesMenuRegions(int* count) {
    if (!s_gamesRegionsInit) {
        for (int i = 0; i < kGamesCount; i++) {
            s_gamesRegions[i] = Region{
                Rect{0, (int16_t)(theme::LIST_START_Y + i * theme::LIST_ITEM_HEIGHT),
                     (int16_t)cfg::DISPLAY_WIDTH, (int16_t)theme::LIST_ITEM_HEIGHT},
                drawGamesRow, nullptr, &s_gamesRowState[i], sizeof(uint8_t), theme::COLOR_BG, {}, false};
        }
        s_gamesRegionsInit = true;
    }
    for (int i = 0; i < kGamesCount; i++) {
        s_gamesRowState[i] = (uint8_t)i | (i == s_gamesSelected ? 0x80 : 0);
    }
    *count = kGamesCount;
    return s_gamesRegions;
}

static Screen kGamesMenuScreen{nullptr, gamesMenuRegions, "Game Session"};

void gamesMenuEnter() {
    s_gamesSelected = 0;
    invalidate();
}

AppState gamesMenuFrame() {
    if (input::wasPressed(input::Button::JoyUp)) {
        s_gamesSelected = (s_gamesSelected - 1 + kGamesCount) % kGamesCount;
        widgets::navSfx();
    }
    if (input::wasPressed(input::Button::JoyDown)) {
        s_gamesSelected = (s_gamesSelected + 1) % kGamesCount;
        widgets::navSfx();
    }
    if (input::wasPressed(input::Button::Back)) {
        return AppState::MainMenu;
    }

    frame(kGamesMenuScreen);

    bool confirm = input::wasPressed(input::Button::Ok) ||
                   input::wasPressed(input::Button::JoySelect);
    if (confirm) {
        switch (s_gamesSelected) {
            case 0: return AppState::Tetris;
            case 1: return AppState::Snake;
            case 2: return AppState::SpaceShooter;
            case 3: return AppState::Game2048;
            case 4: return AppState::Doom;
            case 5: return AppState::Pokemon;
        }
    }
    return AppState::GamesMenu;
}

} // namespace ui
