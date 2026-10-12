#include "network_manager.h"
#include "config.h"

NetworkManager networkManager;

NetworkManager::NetworkManager()
    : wifiSsid(nullptr), wifiPassword(nullptr),
      lastReconnectAttempt(0), connectedState(false) {}

void NetworkManager::init(const char* ssid, const char* password) {
    wifiSsid = ssid;
    wifiPassword = password;

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    Serial.print("[NETWORK] Connecting to Wi-Fi SSID: ");
    Serial.println(wifiSsid);

    if (wifiPassword && strlen(wifiPassword) > 0) {
        WiFi.begin(wifiSsid, wifiPassword);
    } else {
        WiFi.begin(wifiSsid);
    }

    // Initial non-blocking check
    unsigned long startWait = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startWait < 5000)) {
        delay(250);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        connectedState = true;
        Serial.println("\n[NETWORK] Wi-Fi Connected!");
        Serial.print("[NETWORK] IP Address: ");
        Serial.println(WiFi.localIP());
    } else {
        connectedState = false;
        Serial.println("\n[NETWORK] Wi-Fi connection pending. Continuing in offline-ready state.");
    }
}

void NetworkManager::update() {
    bool currentConnected = (WiFi.status() == WL_CONNECTED);

    if (currentConnected != connectedState) {
        connectedState = currentConnected;
        if (connectedState) {
            Serial.print("[NETWORK] Wi-Fi link re-established. IP: ");
            Serial.println(WiFi.localIP());
        } else {
            Serial.println("[NETWORK] Wi-Fi link lost! Falling back to offline queue.");
        }
    }

    // Periodic reconnect attempt if link dropped
    if (!connectedState && (millis() - lastReconnectAttempt >= WIFI_RECONNECT_INTERVAL_MS)) {
        lastReconnectAttempt = millis();
        Serial.println("[NETWORK] Attempting Wi-Fi reconnect...");
        WiFi.reconnect();
    }
}

bool NetworkManager::isConnected() const {
    return (WiFi.status() == WL_CONNECTED);
}

String NetworkManager::getLocalIp() const {
    if (isConnected()) {
        return WiFi.localIP().toString();
    }
    return "0.0.0.0";
}

int NetworkManager::getRssi() const {
    if (isConnected()) {
        return WiFi.RSSI();
    }
    return -100;
}
