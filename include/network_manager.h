#pragma once
#ifndef HOSTELHIVE_NETWORK_MANAGER_H
#define HOSTELHIVE_NETWORK_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>

class NetworkManager {
public:
    NetworkManager();
    void init(const char* ssid, const char* password);
    void update();  // Non-blocking background keepalive
    bool isConnected() const;
    String getLocalIp() const;
    int getRssi() const;

private:
    const char* wifiSsid;
    const char* wifiPassword;
    unsigned long lastReconnectAttempt;
    bool connectedState;
};

extern NetworkManager networkManager;

#endif // HOSTELHIVE_NETWORK_MANAGER_H
