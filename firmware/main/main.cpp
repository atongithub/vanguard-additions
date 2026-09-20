// Without this include, initArduino() frees the BLE controller's memory region (thinking
// no BLE library is linked, since this project uses h2zero/esp-nimble-cpp directly rather
// than a library it recognizes), causing a LoadProhibited crash in NimBLEDevice::init().
#include "esp32-hal-alloc-ble-mem.h"

#include <Arduino.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "../include/state.h"
#include "display/display.h"
#include "input/input.h"
#include "leds/led_manager.h"
#include "audio/audio_manager.h"
#include "ui/animation.h"
#include "power/battery.h"
#include "storage/storage.h"
#include "boot/boot.h"
#include "ui/menu.h"
#include "ui/screensaver.h"
#include "ui/8ball.h"
#include "games/pokemon.h"
#include "games/tetris.h"
#include "games/snake.h"
#include "games/space_shooter.h"
#include "games/game2048.h"
#include "games/doom_screen.h"
#include "settings/settings.h"
#include "music/music.h"
#include "contacts/contacts.h"
#include "challenges/challenges.h"
#include "challenges/console.h"
#include "challenges/challenge_engine.h"
#include "challenges/mission_complete.h"
#include "rf/lora.h"
#include "rf/radio_link.h"
#include "settings/badge_id.h"
#include "radiochat/radio_chat.h"
#include "glitch/glitch.h"
#include "ui/pop_cig.h"
#include "girlfriend/girlfriend.h"

static AppState s_state = AppState::Boot;

static void enterState(AppState s) {
    // Belt-and-suspenders: (removed Peerdrop here)

    // radiolink (Radio Chat) owns the radio while active and must hand it
    // back on every transition that isn't INTO those states, not just an explicit Back
    // press — the DISP shortcut and MissionComplete takeover jump straight to
    // enterState() without running the current screen's own frame() cleanup.
    if (s != AppState::RadioChat) {
        radiolink::end();
    }

    // Challenge-progress LED baseline competes with each game's own LED feedback for the
    // same 14 LEDs; suppressed during gameplay. RadioChat included too since its white-only
    // LED design would otherwise show the red/green progress pattern as unrelated feedback.
    bool suppressBaseline = (s == AppState::Tetris || s == AppState::Snake ||
                             s == AppState::SpaceShooter || s == AppState::Game2048 ||
                             s == AppState::Doom || s == AppState::RadioChat);
    led::setBaselineSuppressed(suppressBaseline);

    switch (s) {
        case AppState::Boot: boot::enter(); break;
        case AppState::Screensaver: ui::screensaver::enter(); break;
        case AppState::MainMenu: ui::mainMenuEnter(); break;
        case AppState::GamesMenu: ui::gamesMenuEnter(); break;
        case AppState::EightBall: ui::eightball::enter(); break;
        case AppState::Pokemon:   games::pokemon::enter(); break;
        case AppState::Tetris:    games::tetris::enter(); break;
        case AppState::Snake: games::snake::enter(); break;
        case AppState::SpaceShooter: games::space_shooter::enter(); break;
        case AppState::Game2048: games::game2048::enter(); break;
        case AppState::Doom: games::doom::enter(); break;
        case AppState::Settings: settings::enter(); break;
        case AppState::ProfileSetup: settings::enter(/*startAtWifiSetup=*/true); break;
        case AppState::MusicPlayer: music::enter(); break;
        case AppState::Contacts: contacts::enter(); break;
        case AppState::Challenges: challenges::enter(); break;
        case AppState::RadioChat: radiochat::enter(); break;
        case AppState::MissionComplete: mission_complete::enter(); break;
        case AppState::PopCig: ui::pop_cig::enter(); break;
        case AppState::GirlfriendMiku: girlfriend::enterMiku(); break;
        case AppState::GirlfriendPaari: girlfriend::enterPaari(); break;
        default: break;
    }
}

static AppState runFrame(AppState s) {
    switch (s) {
        case AppState::Boot: return boot::frame();
        case AppState::Screensaver: return ui::screensaver::frame();
        case AppState::MainMenu: return ui::mainMenuFrame();
        case AppState::GamesMenu: return ui::gamesMenuFrame();
        case AppState::EightBall: return ui::eightball::frame();
        case AppState::Pokemon:   return games::pokemon::frame();
        case AppState::Tetris:    return games::tetris::frame();
        case AppState::Snake: return games::snake::frame();
        case AppState::SpaceShooter: return games::space_shooter::frame();
        case AppState::Game2048: return games::game2048::frame();
        case AppState::Doom: return games::doom::frame();
        case AppState::Settings: return settings::frame();
        case AppState::ProfileSetup: return settings::frame();
        case AppState::MusicPlayer: return music::frame();
        case AppState::Contacts: return contacts::frame();
        case AppState::Challenges: return challenges::frame();
        case AppState::RadioChat: return radiochat::frame();
        case AppState::MissionComplete: return mission_complete::frame();
        case AppState::Glitched: return glitch::frame();
        case AppState::PopCig: return ui::pop_cig::frame();
        case AppState::GirlfriendMiku: return girlfriend::frame();
        case AppState::GirlfriendPaari: return girlfriend::frame();
        default: return s;
    }
}

#if defined(VANGUARD_SATELLITE_SIM)
// Satellite-simulator build: idf.py -DVANGUARD_SATELLITE_SIM=1 -B build_sat build
void satelliteSetup();
void satelliteLoop();
void setup() { satelliteSetup(); }
void loop()  { satelliteLoop(); }
#else

void setup() {
    Serial.begin(115200);

    // Defensive re-check of NVS: initArduino()'s own nvs_flash_init() auto-recovery only
    // handles NO_FREE_PAGES/NEW_VERSION_FOUND; any other failure (e.g. a badge previously
    // flashed with a different NVS layout) leaves NVS permanently uninitialized otherwise.
    esp_err_t nvsErr = nvs_flash_init();
    if (nvsErr != ESP_OK) {
        if (Serial) {
            Serial.printf("[BOOT] nvs_flash_init failed (err=%d) -- erasing NVS partition and retrying...\n", nvsErr);
        }
        nvs_flash_erase();
        nvsErr = nvs_flash_init();
        if (Serial) {
            Serial.printf("[BOOT] nvs_flash_init retry: %s\n", nvsErr == ESP_OK ? "OK" : "FAILED");
        }
    }

    // Silences gpio/ledc driver calls that otherwise log a full line at INFO level every
    // battery ADC sample (~5s), interleaving noise into the serial console the player is
    // reading/typing on. Other tags keep their default log level.
    esp_log_level_set("gpio", ESP_LOG_WARN);
    esp_log_level_set("ledc", ESP_LOG_WARN);

    // Must run before display::init(): TFT/LoRa/shift-registers share MOSI/SCK, and
    // RadioLib only raises LoRa's NSS inside rf::init() (too late) — without this,
    // display::init()'s fillScreen() wedges the SX1261 with LoRa's NSS still floating.
    rf::deselect();

    // Boot-progress trace so a hang (rare, seen once post-flash) shows which setup() step
    // was last to complete. `if (Serial)` avoids the native USB-CDC blocking-write freeze.
    if (Serial) Serial.println("[BOOT] display::init()...");
    display::init();
    if (Serial) Serial.println("[BOOT] input::init()...");
    input::init();
    if (Serial) Serial.println("[BOOT] led::init()...");
    led::init();
    if (Serial) Serial.println("[BOOT] audio::init()...");
    audio::init();
    if (Serial) Serial.println("[BOOT] power::init()...");
    power::init();
    if (Serial) Serial.println("[BOOT] rf::init()...");
    rf::init(); // confirms LoRa SPI-alive before boot animation (spec §1)
    display::setBrightness(storage::loadBrightness());
    if (Serial) Serial.println("[BOOT] storage::initFat()...");
    if (!storage::initFat() && Serial) {
        Serial.println("[BOOT] WARNING: FFat mount failed (no /fat available)");
    }

    if (Serial) Serial.println("[BOOT] challenges::init()...");
    challenges::init(); // starts Level 1's always-on hidden UART leak; restores LED progress from NVS

    if (Serial) Serial.println("[BOOT] setup() complete, entering main loop");
    enterState(s_state);
}

void loop() {
    console::pollConnection(); // one-time welcome banner on a fresh PuTTY/serial connection
    input::update();
    power::update();
    audio::update();
    led::update();
    anim::update();
    challenges::update(); // Level 1's hidden UART leak must keep transmitting regardless of AppState

    // Mission-complete finale: highest priority in loop(), pre-empts whatever screen the
    // player was on. consumeMissionCompleteEvent() fires exactly once ever, so it can't re-steal focus.
    if (challenges::consumeMissionCompleteEvent()) {
        // Forced transition jumps straight to enterState() without running the current
        // screen's own frame() exit cleanup, so audio/LED/WiFi-portal/BLE are stopped
        // unconditionally here instead (idempotent no-ops if not active).
        audio::stop();
        led::stop();
        settings::stopWifiPortalIfActive();
        s_state = AppState::MissionComplete;
        enterState(s_state);
    } else if (glitch::consumeTriggerEvent()) {
        // "Badge Attack" takeover can only fire from RadioChat/ShipBattle. Deliberately
        // bypasses enterState() both ways, since that would tear down radiolink and reset
        // the interrupted screen's state — this way control resumes exactly where it left off.
        glitch::enter(s_state);
        s_state = AppState::Glitched;
    } else if (s_state != AppState::Settings && s_state != AppState::ProfileSetup &&
               s_state != AppState::Pokemon &&
               input::wasPressed(input::Button::Disp)) {
        // DISP (S2): global shortcut to Display settings from anywhere. Excludes
        // ProfileSetup, and Pokemon (which uses it to reboot).
        audio::stop();
        led::stop();
        s_state = AppState::Settings;
        enterState(s_state);
    } else {
        AppState next = runFrame(s_state);
        if (next != s_state) {
            // Leaving Glitched skips enterState(): glitch::frame() returns the exact
            // state it interrupted, whose own state was never touched, so re-entering
            // it would just reset it.
            bool wasGlitched = (s_state == AppState::Glitched);
            s_state = next;
            if (!wasGlitched) enterState(s_state);
        }
    }

    // ESP-IDF's task watchdog panics if IDLE never runs; a 1-tick delay lets it run
    // without perceptible input/render latency.
    delay(1);
}

#endif // VANGUARD_SATELLITE_SIM
