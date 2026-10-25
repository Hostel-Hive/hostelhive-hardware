#pragma once
#ifndef HOSTELHIVE_NETWORK_MANAGER_H
#define HOSTELHIVE_NETWORK_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

class NetworkManager {
public:
    NetworkManager();
    void init(const char* ssid, const char* password);
    void update();  // Non-blocking background keepalive
    bool isConnected() const;
    String getLocalIp() const;
    int getRssi() const;

    // FR012: Exact timestamp logging
    void initTimeSync(long gmtOffsetSec, int daylightOffsetSec, const char* ntpServer);
    bool isTimeSynced() const;
    String getIsoTimestamp();

private:
    const char* wifiSsid;
    const char* wifiPassword;
    unsigned long lastReconnectAttempt;
    unsigned long currentBackoffIntervalMs;
    bool connectedState;
    bool timeSynced;
};

extern NetworkManager networkManager;

#endif // HOSTELHIVE_NETWORK_MANAGER_H
