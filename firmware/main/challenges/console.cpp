#include "console.h"
#include <esp_chip_info.h>

namespace console {

// Every public function bails out if nothing has the USB CDC port open.
// HWCDC Serial writes can BLOCK if no host is draining the TX buffer, which
// would freeze the whole (single-threaded) badge with no terminal attached.
// `if (Serial)` reads HWCDC's DTR/connected state to guard against this.
static bool connected() { return (bool)Serial; }

void rule(char c, uint8_t width) {
    if (!connected()) return;
    Serial.print(DIM);
    Serial.print(CYAN);
    for (uint8_t i = 0; i < width; i++) Serial.print(c);
    Serial.println(RESET);
}

void banner(const char* title, const char* subtitle) {
    if (!connected()) return;
    Serial.println();
    rule('=');
    Serial.print(BOLD); Serial.print(CYAN);
    Serial.print("  ");
    Serial.print(title);
    Serial.println(RESET);
    if (subtitle) {
        Serial.print(DIM); Serial.print(WHITE);
        Serial.print("  ");
        Serial.print(subtitle);
        Serial.println(RESET);
    }
    rule('=');
}

static void tagged(const char* color, const char* tag, const char* msg) {
    if (!connected()) return;
    Serial.print(color);
    Serial.print(tag);
    Serial.print(RESET);
    Serial.print(' ');
    Serial.println(msg);
}

void ok(const char* msg)   { tagged(GREEN,  "[ OK ]", msg); }
void info(const char* msg) { tagged(CYAN,   "[ ** ]", msg); }
void warn(const char* msg) { tagged(YELLOW, "[ !! ]", msg); }
void err(const char* msg)  { tagged(RED,    "[ XX ]", msg); }
void step(const char* msg) { tagged(DIM,    "[ >> ]", msg); }

void field(const char* key, const char* value, const char* valueColor) {
    if (!connected()) return;
    Serial.print("   ");
    Serial.print(DIM); Serial.print(WHITE);
    Serial.print(key);
    Serial.print(RESET);
    // Pad to a fixed column so fields line up.
    int pad = 16 - (int)strlen(key);
    for (int i = 0; i < pad; i++) Serial.print(' ');
    Serial.print(": ");
    if (valueColor) Serial.print(valueColor);
    Serial.print(value);
    Serial.println(RESET);
}

void flagBlock(const char* label, const char* flag) {
    if (!connected()) return;
    Serial.println();
    rule('*');
    Serial.print(BOLD); Serial.print(GREEN);
    Serial.print("  ");
    Serial.println(label);
    Serial.print(BOLD); Serial.print(YELLOW);
    Serial.print("  ");
    Serial.println(flag);
    Serial.print(RESET);
    rule('*');
    Serial.println();
}

void prompt(const char* label) {
    if (!connected()) return;
    Serial.println();
    Serial.print(BOLD); Serial.print(CYAN);
    Serial.print(label);
    Serial.println(RESET);
    Serial.print(GREEN);
    Serial.print("> ");
    Serial.print(RESET);
}

void printHardwareSpecs() {
    if (!connected()) return;

    esp_chip_info_t chip;
    esp_chip_info(&chip);

    uint64_t mac = ESP.getEfuseMac();
    char macStr[24];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             (uint8_t)(mac >> 40), (uint8_t)(mac >> 32), (uint8_t)(mac >> 24),
             (uint8_t)(mac >> 16), (uint8_t)(mac >> 8), (uint8_t)mac);

    char chipBuf[96];
    snprintf(chipBuf, sizeof(chipBuf), "%s (rev v%d.%d, %d Cores @ %d MHz)",
             ESP.getChipModel(), chip.revision / 100, chip.revision % 100,
             chip.cores, (int)ESP.getCpuFreqMHz());

    char flashBuf[64];
    snprintf(flashBuf, sizeof(flashBuf), "%u MB (Clock: %u MHz)",
             (unsigned int)(ESP.getFlashChipSize() / (1024 * 1024)),
             (unsigned int)(ESP.getFlashChipSpeed() / 1000000));

    char ramBuf[80];
    snprintf(ramBuf, sizeof(ramBuf), "Total: %u KB | Free: %u KB | Max Block: %u KB",
             (unsigned int)(ESP.getHeapSize() / 1024),
             (unsigned int)(ESP.getFreeHeap() / 1024),
             (unsigned int)(ESP.getMaxAllocHeap() / 1024));

    char psramBuf[48];
    if (ESP.getPsramSize() > 0) {
        snprintf(psramBuf, sizeof(psramBuf), "%u MB", (unsigned int)(ESP.getPsramSize() / (1024 * 1024)));
    } else {
        snprintf(psramBuf, sizeof(psramBuf), "None (Internal SRAM only)");
    }

    Serial.println();
    Serial.print(BOLD); Serial.print(CYAN);
    Serial.println("  --- HARDWARE SPECIFICATIONS ---");
    Serial.print(RESET);
    field("Chipset", chipBuf, YELLOW);
    field("MAC Address", macStr, WHITE);
    field("Flash Memory", flashBuf, WHITE);
    field("SRAM (Heap)", ramBuf, GREEN);
    field("PSRAM", psramBuf, WHITE);
    field("IDF Version", esp_get_idf_version(), WHITE);
    Serial.println();
}

void pollConnection() {
    // Debounced: some USB-CDC drivers bounce DTR briefly during
    // negotiation, which without this printed the welcome banner twice.
    static bool s_stable = false;
    static bool s_lastRaw = false;
    static unsigned long s_lastChangeMs = 0;
    constexpr unsigned long kDebounceMs = 150;

    bool raw = connected();
    if (raw != s_lastRaw) {
        s_lastRaw = raw;
        s_lastChangeMs = millis();
    }
    if (raw != s_stable && millis() - s_lastChangeMs >= kDebounceMs) {
        bool wasConnected = s_stable;
        s_stable = raw;
        if (s_stable && !wasConnected) {
            banner("VANGUARD GROUND TERMINAL", "Serial link established");
            printHardwareSpecs();
            Serial.print(DIM); Serial.print(WHITE);
            Serial.println("  Navigate the badge's on-screen menu to interact --");
            Serial.println("  Challenges, Contacts and PeerDrop all use this console");
            Serial.println("  when open.");
            Serial.print(RESET);
            Serial.println();
        }
    }
}

} // namespace console
