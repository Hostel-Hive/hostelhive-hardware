#pragma once
#ifndef HOSTELHIVE_DISPLAY_DRIVER_H
#define HOSTELHIVE_DISPLAY_DRIVER_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

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
    void showIdleScreen(const String& ipAddress = "", bool wifiConnected = false, int queueCount = 0);
    void showScanningScreen();
    void showVerifyingScreen(const String& studentId);
    void showAccessGranted(const String& studentName, const String& studentId, const String& room);
    void showAccessDenied(const String& reason);
    void showCooldownScreen(unsigned long remainingSeconds);
    void showOfflineScreen(int queuedCount);
    void update();  // Non-blocking screen timer and animation tick
    void clear();

    ScreenState getCurrentState() const { return currentState; }

protected:
    Adafruit_SSD1306 display;
    bool isAvailable;
    ScreenState currentState;
    unsigned long stateHoldUntil;
    unsigned long lastAnimationTick;
    bool blinkState;
};

extern DisplayDriver displayDriver;

#endif // HOSTELHIVE_DISPLAY_DRIVER_H
