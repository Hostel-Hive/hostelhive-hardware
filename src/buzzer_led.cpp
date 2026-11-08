#include "buzzer_led.h"
#include "config.h"

BuzzerLedController feedbackController;

BuzzerLedController::BuzzerLedController()
    : isToneActive(false), toneEndTime(0) {}

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
    digitalWrite(PIN_BUZZER, LOW);

    // Boot chirp
    triggerFeedback(FeedbackType::SYSTEM_BOOT);
}

void BuzzerLedController::setGreenLed(bool state) {
    writeLed(PIN_LED_GREEN, state);
}

void BuzzerLedController::setRedLed(bool state) {
    writeLed(PIN_LED_RED, state);
}

void BuzzerLedController::playTone(unsigned int frequency, unsigned long durationMs) {
    tone(PIN_BUZZER, frequency, durationMs);
    isToneActive = true;
    toneEndTime = millis() + durationMs;
}

void BuzzerLedController::stopTone() {
    noTone(PIN_BUZZER);
    digitalWrite(PIN_BUZZER, LOW);
    isToneActive = false;
}

void BuzzerLedController::triggerFeedback(FeedbackType type) {
    switch (type) {
        case FeedbackType::SUCCESS_GRANTED:
            setGreenLed(true);
            setRedLed(false);
            playTone(2400, 250);  // High pitch short success chirp
            break;

        case FeedbackType::ACCESS_DENIED:
            setGreenLed(false);
            setRedLed(true);
            playTone(450, 600);   // Low pitch alert buzz
            break;

        case FeedbackType::SCAN_COOLDOWN:
            setGreenLed(false);
            setRedLed(true);
            playTone(800, 150);   // Staccato warning beep
            break;

        case FeedbackType::SYSTEM_BOOT:
            setGreenLed(true);
            setRedLed(true);
            playTone(1800, 100);
            delay(120);
            playTone(2200, 150);
            setGreenLed(false);
            setRedLed(false);
            break;

        case FeedbackType::SYSTEM_OFFLINE:
            setGreenLed(false);
            setRedLed(true);
            playTone(600, 300);
            break;

        case FeedbackType::ERROR_ALERT:
            setRedLed(true);
            playTone(350, 800);
            break;
    }
}

void BuzzerLedController::update() {
    if (isToneActive && millis() >= toneEndTime) {
        stopTone();
    }
}
