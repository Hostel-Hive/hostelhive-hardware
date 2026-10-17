#include <Arduino.h>
#include "config.h"
#include "buzzer_led.h"
#include "relay_control.h"
#include "display_driver.h"
#include "qr_scanner.h"
#include "network_manager.h"
#include "api_client.h"
#include "offline_queue.h"

// Configuration inclusion (secrets.h with fallback to secrets.example.h)
#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"
#endif

// System Timing & State
unsigned long lastUiRefresh = 0;
unsigned long lastQueueFlushAttempt = 0;
bool lastWifiStatus = false;

void handleManualOverride() {
    Serial.println("[GATE] Manual override button triggered.");
    displayDriver.showAccessGranted("MANUAL OVERRIDE", "GATE_STAFF", "--");
    feedbackController.triggerFeedback(FeedbackType::SUCCESS_GRANTED);
    relayController.unlockGate(GATE_UNLOCK_DURATION_MS);
}

void processIncomingScan(const String& rawData) {
    Serial.printf("[CORE] Incoming scan data: '%s'\n", rawData.c_str());
    displayDriver.showScanningScreen();

    // 1. Parse and sanitize payload
    ScanResult parsed = qrScanner.parsePayload(rawData);
    if (!parsed.isValid) {
        Serial.printf("[CORE] Invalid payload: %s\n", parsed.errorReason.c_str());
        displayDriver.showAccessDenied("INVALID QR CODE");
        feedbackController.triggerFeedback(FeedbackType::ACCESS_DENIED);
        return;
    }

    String studentId = parsed.studentId;

    // 2. Cooldown check (SRS FR014)
    unsigned long remainingCooldown = 0;
    if (qrScanner.isCooldownActive(studentId, remainingCooldown)) {
        Serial.printf("[CORE] Scan ignored. Cooldown active for %s (%lu s remaining)\n", studentId.c_str(), remainingCooldown);
        displayDriver.showCooldownScreen(remainingCooldown);
        feedbackController.triggerFeedback(FeedbackType::SCAN_COOLDOWN);
        return;
    }

    // Register scan in cooldown buffer
    qrScanner.registerScan(studentId);

    // 3. Obtain timestamp (SRS FR012)
    String timestampIso = networkManager.getIsoTimestamp();
    displayDriver.showVerifyingScreen(studentId);

    // 4. Transmission or Offline Caching (SRS FR011, FR015)
    if (networkManager.isConnected()) {
        Serial.println("[CORE] Network active. Dispatching scan to backend API...");
        ScanResponse response = apiClient.dispatchScan(studentId, NODE_ID, GATE_DIRECTION, timestampIso, false);

        if (response.success && response.accessGranted) {
            Serial.printf("[CORE] Access Granted for %s\n", response.studentName.c_str());
            displayDriver.showAccessGranted(response.studentName, studentId, response.roomNumber);
            feedbackController.triggerFeedback(FeedbackType::SUCCESS_GRANTED);
            relayController.unlockGate(GATE_UNLOCK_DURATION_MS);
        } else if (response.httpCode > 0 && !response.accessGranted) {
            Serial.printf("[CORE] Access Denied: %s\n", response.message.c_str());
            displayDriver.showAccessDenied(response.message);
            feedbackController.triggerFeedback(FeedbackType::ACCESS_DENIED);
        } else {
            // Network timeout during dispatch -> buffer offline
            Serial.println("[CORE] API request failed/timed out. Caching to LittleFS.");
            offlineQueue.enqueueScan(studentId, NODE_ID, GATE_DIRECTION, timestampIso);
            displayDriver.showOfflineScreen(offlineQueue.getQueueCount());
            feedbackController.triggerFeedback(FeedbackType::SYSTEM_OFFLINE);
            // In emergency fallback, pulse gate for entry continuity if configured
            relayController.unlockGate(GATE_UNLOCK_DURATION_MS);
        }
    } else {
        // Offline mode: Buffer locally (FR015)
        Serial.println("[CORE] Network offline. Caching scan to LittleFS flash buffer.");
        offlineQueue.enqueueScan(studentId, NODE_ID, GATE_DIRECTION, timestampIso);
        displayDriver.showOfflineScreen(offlineQueue.getQueueCount());
        feedbackController.triggerFeedback(FeedbackType::SYSTEM_OFFLINE);
        relayController.unlockGate(GATE_UNLOCK_DURATION_MS);
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("\n=======================================================");
    Serial.println("  HOSTELHIVE IoT ATTENDANCE NODE FIRMWARE");
    Serial.printf("  Version: %s | Node: %s | Mode: %s\n", HOSTELHIVE_FIRMWARE_VERSION, NODE_ID, GATE_DIRECTION);
    Serial.println("=======================================================");

    // Initialize all hardware peripherals
    feedbackController.init();
    relayController.init();
    displayDriver.init();
    qrScanner.init();
    offlineQueue.init();

    // Pin mode for manual override button
    pinMode(PIN_BUTTON_OVERRIDE, INPUT_PULLUP);

    // Initialize Network and API
    networkManager.init(WIFI_SSID, WIFI_PASSWORD);
    if (networkManager.isConnected()) {
        networkManager.initTimeSync(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
    }
    apiClient.init(API_SERVER_HOST, API_ENDPOINT_ATTENDANCE, DEVICE_API_KEY);

    Serial.println("[SETUP] Initialization completed. Ready for scans.\n");
    displayDriver.showIdleScreen(networkManager.getLocalIp(), networkManager.isConnected(), offlineQueue.getQueueCount());
}

void loop() {
    // 1. Subsystem background updates
    feedbackController.update();
    relayController.update();
    displayDriver.update();
    networkManager.update();

    // 2. Manual override button check
    if (digitalRead(PIN_BUTTON_OVERRIDE) == LOW) {
        handleManualOverride();
        delay(400); // Debounce
    }

    // 3. Scan input detection
    if (qrScanner.hasData()) {
        String rawData = qrScanner.readRawPayload();
        if (rawData.length() > 0) {
            processIncomingScan(rawData);
        }
    }

    // 4. Offline queue auto-flush on reconnection (FR015)
    bool currentWifi = networkManager.isConnected();
    if (currentWifi && offlineQueue.getQueueCount() > 0) {
        if (millis() - lastQueueFlushAttempt >= 10000) {
            lastQueueFlushAttempt = millis();
            int flushed = offlineQueue.flushQueue(apiClient);
            if (flushed > 0) {
                displayDriver.showIdleScreen(networkManager.getLocalIp(), true, offlineQueue.getQueueCount());
            }
        }
    }

    // 5. Periodic UI status refresh when idle
    if (displayDriver.getCurrentState() == ScreenState::IDLE) {
        if (millis() - lastUiRefresh >= 5000 || currentWifi != lastWifiStatus) {
            lastUiRefresh = millis();
            lastWifiStatus = currentWifi;
            displayDriver.showIdleScreen(networkManager.getLocalIp(), currentWifi, offlineQueue.getQueueCount());
        }
    }
}
