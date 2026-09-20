#include "challenge_engine.h"
#include "challenge_data.h"
#include "challenge_state.h"
#include "uart_leak.h"
#include "../leds/led_manager.h"
#include "../audio/audio_manager.h"
#include <Arduino.h>

namespace challenges {

void init() {
    setCompleted("lvl1", true);
    uartleak::init(); // Level 1's always-on hidden UART leak
    led::setChallengeProgress(completedCount()); // restore LED progress from NVS on boot
}

void update() {
    uartleak::update();
}

bool isUnlocked(int index) {
    const char* req = kRegistry[index].requiresId;
    if (!req) return true;
    return isCompleted(req);
}

int completedCount() {
    int n = 0;
    for (int i = 0; i < kStageCount; i++) {
        if (kRegistry[i].kind == StageKind::FlagDesk) continue;
        if (isCompleted(kRegistry[i].id)) n++;
    }
    return n;
}

static const audio::Note kCompletionMelody[] = {
    { 1318, 90 }, { 1568, 90 }, { 1976, 90 }, { 2637, 220 },
};

// Guards consumeMissionCompleteEvent() to fire only once per completion;
// resetAll() clears it so a second full run can trigger it again.
static bool s_missionCompleteFired = false;
static bool s_missionCompletePending = false;

void completeChallenge(const char* id) {
    if (isCompleted(id)) return; // idempotent — re-submitting a flag is a no-op past the first time
    setCompleted(id, true);
    led::setChallengeProgress(completedCount());
    led::playEffect(led::EffectId::SuccessFlash, led::EffectParams{0, 0, 0, led::kMaskAll});
    audio::playMelody(kCompletionMelody, 4, false);

    if (!s_missionCompleteFired && completedCount() == kStageCount - 1 /* excludes the FlagDesk utility entry */) {
        s_missionCompleteFired = true;
        s_missionCompletePending = true;
    }
}

bool consumeMissionCompleteEvent() {
    if (!s_missionCompletePending) return false;
    s_missionCompletePending = false;
    return true;
}

void resetAll() {
    for (int i = 0; i < kStageCount; i++) {
        if (kRegistry[i].kind == StageKind::FlagDesk) continue;
        setCompleted(kRegistry[i].id, false);
    }
    led::setChallengeProgress(0);
    s_missionCompleteFired = false;
}

} // namespace challenges
