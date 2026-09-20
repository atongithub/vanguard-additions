#pragma once
#include "../../include/state.h"

// The badge's permanent home screen — boot lands here, every major section's Back returns here.
// Shows starfield/logo idle animation; joystick/Select opens Main Menu, Ok opens Profile Setup.
// Its own always-reachable AppState (not an idle-timeout overlay) so it's reachable directly from boot.
namespace ui::screensaver {

void enter();
AppState frame();
void activateSpeedBoost();

} // namespace ui::screensaver
