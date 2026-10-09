#pragma once
#ifndef HOSTELHIVE_BUZZER_LED_H
#define HOSTELHIVE_BUZZER_LED_H

#include <Arduino.h>

enum class FeedbackType {
    SUCCESS_GRANTED,
    ACCESS_DENIED,
    SCAN_COOLDOWN,
    SYSTEM_BOOT,
    SYSTEM_OFFLINE,
    ERROR_ALERT
};

class BuzzerLedController {
public:
    BuzzerLedController();
    void init();
    void triggerFeedback(FeedbackType type);
    void update();  // Non-blocking tick routine
    void setGreenLed(bool state);
    void setRedLed(bool state);

private:
    void playTone(unsigned int frequency, unsigned long durationMs);
    void stopTone();

    bool isToneActive;
    unsigned long toneEndTime;
};

extern BuzzerLedController feedbackController;

#endif // HOSTELHIVE_BUZZER_LED_H
