#pragma once
#ifndef HOSTELHIVE_DISPLAY_DRIVER_H
#define HOSTELHIVE_DISPLAY_DRIVER_H

#include <Arduino.h>
#include "config.h"

#if HAS_OLED
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#endif

// With HAS_OLED 0 (NodeMCU build) the same screens are written as one-line
// "[GATE] ..." status messages on the USB serial log instead.

enum class ScreenState {
    BOOT,
    IDLE,
    SCANNING,
    VERIFYING,
    GRANTED,
    DENIED,
    COOLDOWN,
    OFFLINE
};

class DisplayDriver {
public:
    DisplayDriver();
    bool init();
    void showBootScreen();
    void showIdleScreen(const String& ipAddress = "", bool wifiConnected = false, unsigned long overrideCount = 0);
    void showScanningScreen();
    void showVerifyingScreen(const String& studentId);
    void showAccessGranted(const String& studentName, const String& studentId, const String& room);
    void showAccessDenied(const String& reason);
    void showCooldownScreen(unsigned long remainingSeconds);
    void showOfflineScreen();
    void update();  // Non-blocking screen timer and animation tick
    void clear();

    ScreenState getCurrentState() const { return currentState; }

protected:
#if HAS_OLED
    Adafruit_SSD1306 display;
#else
    void logStatus(const String& line);
    String lastLine;
    unsigned long lastLineAt;
#endif
    bool isAvailable;
    ScreenState currentState;
    unsigned long stateHoldUntil;
    unsigned long lastAnimationTick;
    bool blinkState;
};

extern DisplayDriver displayDriver;

#endif // HOSTELHIVE_DISPLAY_DRIVER_H
