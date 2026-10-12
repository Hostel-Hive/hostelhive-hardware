#include "network_manager.h"
#include "config.h"

NetworkManager networkManager;

NetworkManager::NetworkManager()
    : wifiSsid(nullptr), wifiPassword(nullptr),
      lastReconnectAttempt(0), currentBackoffIntervalMs(WIFI_RECONNECT_INTERVAL_MS),
      connectedState(false), timeSynced(false) {}

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

    unsigned long startWait = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startWait < 5000)) {
        delay(250);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        connectedState = true;
        currentBackoffIntervalMs = WIFI_RECONNECT_INTERVAL_MS;
        Serial.println("\n[NETWORK] Wi-Fi Connected!");
        Serial.print("[NETWORK] IP Address: ");
        Serial.println(WiFi.localIP());
    } else {
        connectedState = false;
        Serial.println("\n[NETWORK] Wi-Fi connection pending. Continuing in offline-ready state.");
    }
}

void NetworkManager::initTimeSync(long gmtOffsetSec, int daylightOffsetSec, const char* ntpServer) {
    if (!isConnected()) {
        Serial.println("[NTP] Wi-Fi not ready. NTP sync deferred.");
        return;
    }

    Serial.println("[NTP] Synchronizing internal RTC via SNTP...");
    configTime(gmtOffsetSec, daylightOffsetSec, ntpServer);

    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 3000)) {
        timeSynced = true;
        Serial.print("[NTP] Time synchronized successfully: ");
        char buf[64];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
        Serial.println(buf);
    } else {
        timeSynced = false;
        Serial.println("[NTP] Failed to obtain time from NTP server.");
    }
}

bool NetworkManager::isTimeSynced() const {
    return timeSynced;
}

String NetworkManager::getIsoTimestamp() {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 100)) {
        timeSynced = true;
        char buffer[32];
        strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S+05:30", &timeinfo);
        return String(buffer);
    }

    unsigned long uptimeSec = millis() / 1000;
    char fallback[32];
    snprintf(fallback, sizeof(fallback), "1970-01-01T00:%02lu:%02lu+05:30", uptimeSec / 60, uptimeSec % 60);
    return String(fallback);
}

void NetworkManager::update() {
    bool currentConnected = (WiFi.status() == WL_CONNECTED);

    if (currentConnected != connectedState) {
        connectedState = currentConnected;
        if (connectedState) {
            Serial.print("[NETWORK] Wi-Fi link re-established. IP: ");
            Serial.println(WiFi.localIP());
            currentBackoffIntervalMs = WIFI_RECONNECT_INTERVAL_MS; // Reset backoff
        } else {
            Serial.println("[NETWORK] Wi-Fi link lost! Falling back to offline buffer.");
        }
    }

    // Exponential backoff reconnect
    if (!connectedState && (millis() - lastReconnectAttempt >= currentBackoffIntervalMs)) {
        lastReconnectAttempt = millis();
        Serial.print("[NETWORK] Reconnecting (backoff ");
        Serial.print(currentBackoffIntervalMs / 1000);
        Serial.println("s)...");

        WiFi.reconnect();

        // Increment backoff up to 60s
        if (currentBackoffIntervalMs < 60000) {
            currentBackoffIntervalMs = min(currentBackoffIntervalMs * 2, (unsigned long)60000);
        }
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
