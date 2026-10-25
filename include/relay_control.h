#pragma once
#ifndef HOSTELHIVE_RELAY_CONTROL_H
#define HOSTELHIVE_RELAY_CONTROL_H

#include <Arduino.h>

class RelayController {
public:
    RelayController();
    void init();
    void unlockGate(unsigned long durationMs = 3000);
    void lockGate();
    void update();  // Non-blocking tick routine
    bool isGateUnlocked() const;

private:
    bool isUnlocked;
    unsigned long lockTargetTime;
};

extern RelayController relayController;

#endif // HOSTELHIVE_RELAY_CONTROL_H
