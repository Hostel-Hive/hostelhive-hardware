#include "buzzer_led.h"
#include "config.h"

BuzzerLedController feedbackController;

BuzzerLedController::BuzzerLedController()
    : isToneActive(false), toneEndTime(0) {}

void BuzzerLedController::init() {
    pinMode(PIN_LED_GREEN, OUTPUT);
    pinMode(PIN_LED_RED, OUTPUT);
    pinMode(PIN_BUZZER, OUTPUT);

    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_LED_RED, LOW);
    digitalWrite(PIN_BUZZER, LOW);

    // Boot chirp
    triggerFeedback(FeedbackType::SYSTEM_BOOT);
}

void BuzzerLedController::setGreenLed(bool state) {
    digitalWrite(PIN_LED_GREEN, state ? HIGH : LOW);
}

void BuzzerLedController::setRedLed(bool state) {
    digitalWrite(PIN_LED_RED, state ? HIGH : LOW);
}

void BuzzerLedController::playTone(unsigned int frequency, unsigned long durationMs) {
#if defined(ESP32)
    tone(PIN_BUZZER, frequency, durationMs);
#else
    tone(PIN_BUZZER, frequency, durationMs);
#endif
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
