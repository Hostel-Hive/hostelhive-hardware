#pragma once
#ifndef HOSTELHIVE_BUZZER_LED_H
#define HOSTELHIVE_BUZZER_LED_H

#include <Arduino.h>

enum class FeedbackType {
    SCAN_ACK,         // short beep the moment a card is read
    SUCCESS_GRANTED,  // short beep
    ACCESS_DENIED,    // long beep
    SCAN_COOLDOWN,    // 2 very short ticks (QR gate: same pass again)
    SCAN_READY,       // beep-beep: the next card can be tapped
    SYSTEM_BOOT,      // 2 short chirps
    SYSTEM_OFFLINE,   // long beep (a rejection like any other)
    ERROR_ALERT       // extra-long beep
};

// One step of a beep pattern: sound for onMs, then silence for offMs.
// freq is only used by a passive buzzer; an active buzzer makes its own tone.
struct BeepStep {
    uint16_t freq;
    uint16_t onMs;
    uint16_t offMs;
};

class BuzzerLedController {
public:
    BuzzerLedController();
    void init();
    // Sets the LEDs and starts the beep pattern; update() plays the rest.
    void triggerFeedback(FeedbackType type);
    // Plays the whole pattern before returning (boot chirp, "buzz" test command).
    void playBlocking(FeedbackType type);
    void update();  // Non-blocking tick routine: call every loop
    bool isBusy() const { return pattern != nullptr; }
    void setGreenLed(bool state);
    void setRedLed(bool state);

    static const char* name(FeedbackType type);

private:
    void startPattern(const BeepStep* steps, uint8_t count);

    const BeepStep* pattern;
    uint8_t stepCount;
    uint8_t stepIndex;
    bool soundOn;
    unsigned long phaseEndsAt;
};

extern BuzzerLedController feedbackController;

#endif // HOSTELHIVE_BUZZER_LED_H
