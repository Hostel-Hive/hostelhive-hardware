#include "buzzer_led.h"
#include "config.h"

BuzzerLedController feedbackController;

// Beep patterns. A scan goes: short beep (card read) -> silence while the
// backend answers -> short beep (granted) or long beep (rejected); after a
// granted scan, beep-beep once the gate has closed and the next card can tap.
static const BeepStep PATTERN_ACK[] = {{2400, 80, 0}};
static const BeepStep PATTERN_GRANTED[] = {{2400, 150, 0}};
static const BeepStep PATTERN_DENIED[] = {{450, 800, 0}};
static const BeepStep PATTERN_COOLDOWN[] = {{800, 40, 80}, {800, 40, 0}};
static const BeepStep PATTERN_READY[] = {{2000, 80, 100}, {2400, 80, 0}};
static const BeepStep PATTERN_BOOT[] = {{1800, 60, 80}, {2200, 60, 0}};
static const BeepStep PATTERN_ERROR[] = {{350, 1200, 0}};

#define PATTERN(p) p, (uint8_t)(sizeof(p) / sizeof(p[0]))

static void buzzerOn(uint16_t freq, uint32_t durationMs) {
#if BUZZER_ACTIVE
    (void)freq;
    (void)durationMs;
    digitalWrite(PIN_BUZZER, HIGH);
#else
    // The duration lets the tone stop on time even if loop() is busy.
    tone(PIN_BUZZER, freq, durationMs);
#endif
}

static void buzzerOff() {
#if !BUZZER_ACTIVE
    noTone(PIN_BUZZER);
#endif
    digitalWrite(PIN_BUZZER, LOW);
}


BuzzerLedController::BuzzerLedController()
    : pattern(nullptr), stepCount(0), stepIndex(0), soundOn(false), phaseEndsAt(0) {}

// A pin of -1 means the LED is not fitted on this board.
static void writeLed(int pin, bool on) {
    if (pin < 0) return;
    digitalWrite(pin, on ? LED_ACTIVE_LEVEL : !LED_ACTIVE_LEVEL);
}

void BuzzerLedController::init() {
    if (PIN_LED_GREEN >= 0) pinMode(PIN_LED_GREEN, OUTPUT);
    if (PIN_LED_RED >= 0) pinMode(PIN_LED_RED, OUTPUT);
    pinMode(PIN_BUZZER, OUTPUT);

    setGreenLed(false);
    setRedLed(false);
    buzzerOff();

    // Played to the end here: setup() then blocks on Wi-Fi, and an active
    // buzzer left on would sound for the whole wait.
    playBlocking(FeedbackType::SYSTEM_BOOT);
}

void BuzzerLedController::setGreenLed(bool state) {
    writeLed(PIN_LED_GREEN, state);
}

void BuzzerLedController::setRedLed(bool state) {
    writeLed(PIN_LED_RED, state);
}

void BuzzerLedController::startPattern(const BeepStep* steps, uint8_t count) {
    buzzerOff();
    pattern = steps;
    stepCount = count;
    stepIndex = 0;
    soundOn = true;
    buzzerOn(steps[0].freq, steps[0].onMs);
    phaseEndsAt = millis() + steps[0].onMs;
}

void BuzzerLedController::update() {
    if (pattern == nullptr || (long)(millis() - phaseEndsAt) < 0) return;

    if (soundOn) {
        buzzerOff();
        soundOn = false;
        phaseEndsAt = millis() + pattern[stepIndex].offMs;
        return;
    }
    stepIndex++;
    if (stepIndex >= stepCount) {
        pattern = nullptr;
        return;
    }
    soundOn = true;
    buzzerOn(pattern[stepIndex].freq, pattern[stepIndex].onMs);
    phaseEndsAt = millis() + pattern[stepIndex].onMs;
}

void BuzzerLedController::playBlocking(FeedbackType type) {
    triggerFeedback(type);
    while (isBusy()) {
        update();
        delay(2);
    }
}

void BuzzerLedController::triggerFeedback(FeedbackType type) {
    switch (type) {
        case FeedbackType::SCAN_ACK:
            startPattern(PATTERN(PATTERN_ACK));
            break;

        case FeedbackType::SUCCESS_GRANTED:
            setGreenLed(true);
            setRedLed(false);
            startPattern(PATTERN(PATTERN_GRANTED));
            break;

        case FeedbackType::ACCESS_DENIED:
            setGreenLed(false);
            setRedLed(true);
            startPattern(PATTERN(PATTERN_DENIED));
            break;

        case FeedbackType::SCAN_COOLDOWN:
            setGreenLed(false);
            setRedLed(true);
            startPattern(PATTERN(PATTERN_COOLDOWN));
            break;

        case FeedbackType::SCAN_READY:
            setGreenLed(false);
            setRedLed(false);
            startPattern(PATTERN(PATTERN_READY));
            break;

        case FeedbackType::SYSTEM_BOOT:
            startPattern(PATTERN(PATTERN_BOOT));
            break;

        case FeedbackType::SYSTEM_OFFLINE:
            setGreenLed(false);
            setRedLed(true);
            startPattern(PATTERN(PATTERN_DENIED));
            break;

        case FeedbackType::ERROR_ALERT:
            setRedLed(true);
            startPattern(PATTERN(PATTERN_ERROR));
            break;
    }
}

const char* BuzzerLedController::name(FeedbackType type) {
    switch (type) {
        case FeedbackType::SCAN_ACK: return "card read (short beep)";
        case FeedbackType::SUCCESS_GRANTED: return "granted (short beep)";
        case FeedbackType::ACCESS_DENIED: return "rejected (long beep)";
        case FeedbackType::SCAN_COOLDOWN: return "duplicate scan (2 ticks)";
        case FeedbackType::SCAN_READY: return "ready for next card (beep-beep)";
        case FeedbackType::SYSTEM_BOOT: return "boot (2 short chirps)";
        case FeedbackType::SYSTEM_OFFLINE: return "offline (long beep)";
        case FeedbackType::ERROR_ALERT: return "error (extra-long beep)";
    }
    return "";
}
