#pragma once
#ifndef HOSTELHIVE_DISPLAY_DRIVER_H
#define HOSTELHIVE_DISPLAY_DRIVER_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

class DisplayDriver {
public:
    DisplayDriver();
    bool init();
    void showBootScreen();
    void clear();

protected:
    Adafruit_SSD1306 display;
    bool isAvailable;
};

extern DisplayDriver displayDriver;

#endif // HOSTELHIVE_DISPLAY_DRIVER_H
