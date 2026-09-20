#pragma once
#include <Arduino.h>

// Shared serial-console styling for challenge screens over PuTTY.
// ASCII-only box drawing: PuTTY's charset isn't guaranteed UTF-8. SGR color
// is charset-independent so it's safe. Degrades gracefully if escapes are ignored.
namespace console {

// ---- ANSI SGR sequences -------------------------------------------------
constexpr const char* RESET  = "\033[0m";
constexpr const char* BOLD   = "\033[1m";
constexpr const char* DIM    = "\033[2m";
constexpr const char* RED    = "\033[31m";
constexpr const char* GREEN  = "\033[32m";
constexpr const char* YELLOW = "\033[33m";
constexpr const char* BLUE   = "\033[34m";
constexpr const char* CYAN   = "\033[36m";
constexpr const char* WHITE  = "\033[37m";

// Full-width rule, e.g. "========================================".
void rule(char c = '=', uint8_t width = 52);

// Framed title block:
//   ====================================================
//    MISSION 03 :: ESTABLISHING THE UPLINK
//   ====================================================
// `subtitle` may be nullptr.
void banner(const char* title, const char* subtitle = nullptr);

// Status lines with a consistent, colour-coded prefix.
void ok(const char* msg);        // green  [ OK ]
void info(const char* msg);      // cyan   [ ** ]
void warn(const char* msg);      // yellow [ !! ]
void err(const char* msg);       // red    [ XX ]
void step(const char* msg);      // dim    [ >> ]

// "key : value" aligned detail line, e.g.   Frequency    : 435.500 MHz
void field(const char* key, const char* value, const char* valueColor = nullptr);

// Big success block used when a flag is revealed.
void flagBlock(const char* label, const char* flag);

// The "> " input prompt every interactive screen uses.
void prompt(const char* label);

// Prints full ESP32 hardware specifications to the serial terminal.
void printHardwareSpecs();

// Call once per loop() tick. Detects a PuTTY/serial terminal newly
// connecting (DTR edge) and prints a one-time welcome banner.
void pollConnection();

} // namespace console
