#include "relay_control.h"
#include "config.h"

RelayController relayController;

RelayController::RelayController()
    : isUnlocked(false), lockTargetTime(0) {}

void RelayController::init() {
    pinMode(PIN_RELAY_LOCK, OUTPUT);
    lockGate();
}

void RelayController::unlockGate(unsigned long durationMs) {
    digitalWrite(PIN_RELAY_LOCK, RELAY_ACTIVE_LEVEL);
    isUnlocked = true;
    lockTargetTime = millis() + durationMs;
}

void RelayController::lockGate() {
    digitalWrite(PIN_RELAY_LOCK, (RELAY_ACTIVE_LEVEL == HIGH) ? LOW : HIGH);
    isUnlocked = false;
    lockTargetTime = 0;
}

void RelayController::update() {
    if (isUnlocked && millis() >= lockTargetTime) {
        lockGate();
    }
}

bool RelayController::isGateUnlocked() const {
    return isUnlocked;
}
