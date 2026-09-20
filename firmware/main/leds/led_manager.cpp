#include "led_manager.h"
#include "led_chain.h"
#include <Arduino.h>
#include <math.h>
#include <esp_random.h>

namespace led {

static EffectId s_effect = EffectId::Off;
static EffectParams s_params;
static unsigned long s_effectStartMs = 0;
static unsigned long s_lastStepMs = 0;
static int s_stepIndex = 0;
static bool s_blinkOn = false;

// Center-out pair order for the boot sweep. (Top 2 white LEDs only)
static const int kSweepOrder[5][2] = {
    { 4, 5 }, { 3, 6 }, { 2, 7 }, { 1, 8 },
    { 13, 14 },
};

// Expands a bit-mask into a list of set bit positions, for effects that need an ordered sequence.
static int collectMaskBits(uint16_t mask, int* outBits) {
    int count = 0;
    for (int b = 0; b < 16; b++) {
        if (mask & (1u << b)) outBits[count++] = b;
    }
    return count;
}

// Challenge-progress baseline (resting state, see led_manager.h).
static uint8_t s_challengeCompleted = 0;
static bool s_baselineSuppressed = false;

static uint16_t challengeBaselinePattern() {
    if (s_baselineSuppressed) return 0x0000;

    int bits[16];
    uint16_t value = 0;

    int greenLit = 4 - s_challengeCompleted; // consumed one at a time as challenges complete
    int gCount = collectMaskBits(kMaskGreen, bits);
    for (int i = 0; i < greenLit && i < gCount; i++) value |= (1u << bits[i]);

    int rCount = collectMaskBits(kMaskRed, bits);
    for (int i = 0; i < (int)s_challengeCompleted && i < rCount; i++) value |= (1u << bits[i]);

    return value;
}

static void writeBaseline() {
    leds::writeChainRaw(challengeBaselinePattern());
}

void setChallengeProgress(uint8_t completedCount) {
    s_challengeCompleted = completedCount > 4 ? 4 : completedCount;
    if (s_effect == EffectId::Off) writeBaseline(); // refresh immediately if currently idle
}

void setBaselineSuppressed(bool suppressed) {
    if (s_baselineSuppressed == suppressed) return;
    s_baselineSuppressed = suppressed;
    if (s_effect == EffectId::Off) writeBaseline(); // refresh immediately if currently idle
}

static void applyPitchVisualizer(uint16_t freqHz) {
    leds::clearAll();
    if (freqHz == 0) {
        // rest — no LEDs lit
    } else if (freqHz < 450) {
        for (int i = 1; i <= 4; i++) leds::setChainLed(i, true); // bass -> red center
    } else if (freqHz < 900) {
        for (int i = 5; i <= 8; i++) leds::setChainLed(i, true); // mid -> green center
    } else {
        for (int i = 9; i <= 14; i++) leds::setChainLed(i, true); // treble -> white sides
    }
}

void init() {
    leds::init();
}

void playEffect(EffectId id, EffectParams params) {
    if (id == EffectId::Off) {
        id = EffectId::Sweep;
        params = EffectParams{0, 0, 80, (uint16_t)(kMaskRed | kMaskGreen)};
    }

    // SuccessFlash/FailurePulse are presets of ChallengeComplete's flash mechanism;
    // resolve defaults here so stepChallengeComplete() only reads s_params.
    if (id == EffectId::SuccessFlash) {
        if (!params.mask) params.mask = kMaskGreen;
        if (!params.flashCount) params.flashCount = 2;
        if (!params.periodMs) params.periodMs = 90;
    } else if (id == EffectId::FailurePulse) {
        if (!params.mask) params.mask = kMaskRed;
        if (!params.flashCount) params.flashCount = 3;
        if (!params.periodMs) params.periodMs = 180;
    }

    s_effect = id;
    s_params = params;
    s_effectStartMs = millis();
    s_lastStepMs = s_effectStartMs;
    s_stepIndex = 0;
    s_blinkOn = false;

    switch (id) {
        case EffectId::BootSweep:
            leds::clearAll();
            break;
        case EffectId::PitchVisualizer:
            applyPitchVisualizer(params.freqHz);
            break;
        case EffectId::ChallengeComplete:
        case EffectId::SuccessFlash:
        case EffectId::FailurePulse:
            leds::clearAll();
            break;
        default:
            break;
    }
}

void stop() {
    playEffect(EffectId::Off);
}

static void stepBootSweep() {
    const uint16_t period = s_params.periodMs ? s_params.periodMs : 50;
    unsigned long now = millis();
    if (now - s_lastStepMs < period) return;
    s_lastStepMs = now;

    // Flash a random combination of the 14 LEDs
    uint16_t randomMask = esp_random() & 0x7FFE; // Bits 1-14 are the chain LEDs
    leds::writeChainRaw(randomMask);
}

// Shared by ChallengeComplete/SuccessFlash/FailurePulse: N flashes, then self-returns to Off.
static void stepChallengeComplete() {
    const uint16_t period = s_params.periodMs ? s_params.periodMs : 150;
    unsigned long now = millis();
    if (now - s_lastStepMs < period) return;
    s_lastStepMs = now;

    uint8_t flashes = s_params.flashCount ? s_params.flashCount : 3;
    uint16_t mask = s_params.mask ? s_params.mask : kMaskAll;
    int totalSteps = flashes * 2;
    if (s_stepIndex >= totalSteps) {
        playEffect(EffectId::Off);
        return;
    }
    bool on = (s_stepIndex % 2) == 0;
    leds::writeChainRaw(on ? mask : 0x0000);
    s_stepIndex++;
}

// Not Breathing's smooth sine envelope — two overlapping bumps (lead pulse + echo) read
// like a heartbeat, distinguishing "connecting" from idle scanning.
static float pulseBump(float x, float center, float width) {
    float d = (x - center) / width;
    float v = 1.0f - d * d;
    return v > 0.0f ? v : 0.0f;
}

static void stepConnectionPulse() {
    const uint16_t period = s_params.periodMs ? s_params.periodMs : 900;
    uint16_t mask = s_params.mask ? s_params.mask : kMaskAll;
    unsigned long now = millis();
    if (now - s_lastStepMs < 20) return; // ~50Hz cap, same as Breathing
    s_lastStepMs = now;

    int bits[16];
    int count = collectMaskBits(mask, bits);
    if (count == 0) return;

    unsigned long elapsed = (now - s_effectStartMs) % period;
    float t = (float)elapsed / (float)period; // 0..1

    float env = pulseBump(t, 0.08f, 0.09f) + 0.7f * pulseBump(t, 0.28f, 0.11f);
    if (env > 1.0f) env = 1.0f;
    int litCount = (int)(env * count + 0.5f);

    uint16_t value = 0;
    for (int i = 0; i < litCount; i++) value |= (1u << bits[i]);
    leds::writeChainRaw(value);
}

// No per-LED PWM on this shift register, so "breathing" ramps lit-count in a sine
// envelope instead of true brightness.
static void stepBreathing() {
    const uint16_t period = s_params.periodMs ? s_params.periodMs : 1800;
    uint16_t mask = s_params.mask ? s_params.mask : kMaskAll;
    unsigned long now = millis();
    if (now - s_lastStepMs < 20) return; // ~50Hz cap — smooth without spamming SPI
    s_lastStepMs = now;

    int bits[16];
    int count = collectMaskBits(mask, bits);
    if (count == 0) return;

    unsigned long elapsed = (now - s_effectStartMs) % period;
    float phase = (float)elapsed / (float)period;
    float env = (sinf(phase * 2.0f * PI - PI / 2.0f) + 1.0f) / 2.0f; // 0 -> 1 -> 0
    int litCount = (int)(env * count + 0.5f);

    uint16_t value = 0;
    for (int i = 0; i < litCount; i++) value |= (1u << bits[i]);
    leds::writeChainRaw(value);
}

// Two adjacent bits lit per step (no per-LED brightness for a real fading tail);
// approximates a moving trail.
static void stepSweep() {
    const uint16_t period = s_params.periodMs ? s_params.periodMs : 80;
    uint16_t mask = s_params.mask ? s_params.mask : kMaskAll;
    unsigned long now = millis();
    if (now - s_lastStepMs < period) return;
    s_lastStepMs = now;

    int bits[16];
    int count = collectMaskBits(mask, bits);
    if (count == 0) return;

    uint16_t value = 1u << bits[s_stepIndex % count];
    if (count > 1) value |= 1u << bits[(s_stepIndex + count - 1) % count];
    leds::writeChainRaw(value);
    s_stepIndex++;
}

// Points chasing along the masked LEDs — PeerDrop transfer screen's "packets moving" effect.
static void stepPacketFlow() {
    const uint16_t period = s_params.periodMs ? s_params.periodMs : 70;
    uint16_t mask = s_params.mask ? s_params.mask : kMaskAll;
    unsigned long now = millis();
    if (now - s_lastStepMs < period) return;
    s_lastStepMs = now;

    int bits[16];
    int count = collectMaskBits(mask, bits);
    if (count == 0) return;

    constexpr int kPackets = 3;
    constexpr int kSpacing = 2;
    uint16_t value = 0;
    for (int p = 0; p < kPackets; p++) {
        int idx = (s_stepIndex + p * kSpacing) % count;
        value |= (1u << bits[idx]);
    }
    leds::writeChainRaw(value);
    s_stepIndex++;
}

// Screensaver ambient effect: center column (green+red) rises bottom-to-top as one fill;
// white sides switch on once fully lit, hold, then reset. Ignores s_params.mask.
static void stepSideJoin() {
    const uint16_t period = s_params.periodMs ? s_params.periodMs : 140;
    unsigned long now = millis();
    if (now - s_lastStepMs < period) return;
    s_lastStepMs = now;

    constexpr int kCenterLen = 8;  // LED8 (bottom, green) .. LED1 (top, red)
    constexpr int kHoldSteps = 4;  // extra beats held with white lit before restarting
    constexpr int kCycleSteps = kCenterLen + kHoldSteps;
    constexpr uint16_t kSideMask = (1u << 15) | (1u << 14) | (1u << 13) | (1u << 12); // LED9-12, all four side LEDs

    int step = s_stepIndex % kCycleSteps;
    s_stepIndex++;

    // Rising column: once an LED lights it stays lit for the rest of this cycle.
    int litCenter = step < kCenterLen ? step + 1 : kCenterLen;
    uint16_t value = (uint16_t)((1u << litCenter) - 1); // low bits = LED8 upward

    if (step >= kCenterLen) value |= kSideMask; // white only during hold phase

    leds::writeChainRaw(value);
}

static void stepBlink(uint16_t defaultPeriodMs) {
    const uint16_t period = s_params.periodMs ? s_params.periodMs : defaultPeriodMs;
    uint16_t mask = s_params.mask ? s_params.mask : kMaskAll;
    unsigned long now = millis();
    if (now - s_lastStepMs < period / 2) return;
    s_lastStepMs = now;
    s_blinkOn = !s_blinkOn;
    leds::writeChainRaw(s_blinkOn ? mask : 0x0000);
}

static void stepIdle() {
    const uint16_t period = s_params.periodMs ? s_params.periodMs : 2000;
    unsigned long now = millis();
    if (now - s_lastStepMs < period / 2) return;
    s_lastStepMs = now;
    s_blinkOn = !s_blinkOn;
    leds::setBuiltin(s_blinkOn);
}

void update() {
    switch (s_effect) {
        case EffectId::Off:
        case EffectId::PitchVisualizer:
            break; // static/instantaneous, nothing to step
        case EffectId::BootSweep:
            stepBootSweep();
            break;
        case EffectId::ChallengeComplete:
        case EffectId::SuccessFlash:
        case EffectId::FailurePulse:
            stepChallengeComplete();
            break;
        case EffectId::ConnectionPulse:
            stepConnectionPulse();
            break;
        case EffectId::Breathing:
            stepBreathing();
            break;
        case EffectId::Sweep:
            stepSweep();
            break;
        case EffectId::PacketFlow:
            stepPacketFlow();
            break;
        case EffectId::SideJoin:
            stepSideJoin();
            break;
        case EffectId::Notification:
            stepBlink(400);
            break;
        case EffectId::Warning:
            stepBlink(150);
            break;
        case EffectId::Idle:
            stepIdle();
            break;
    }
}

} // namespace led
