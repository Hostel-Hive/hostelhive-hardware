#include <Arduino.h>
#include "config.h"
#include "buzzer_led.h"
#include "relay_control.h"
#include "display_driver.h"
#include "qr_scanner.h"
#include "network_manager.h"
#include "api_client.h"
#include "override_log.h"
#include "gate_policy.h"

// Credentials live only in the git-ignored secrets.h. There is deliberately no
// fallback to the example file: a gate must never boot with a public key.
#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "include/secrets.h is missing. Copy include/secrets.example.h to include/secrets.h and set real values."
#endif

#ifndef ALLOW_INSECURE_HTTP
#define ALLOW_INSECURE_HTTP 0
#endif

// System Timing & State
unsigned long lastUiRefresh = 0;
bool lastWifiStatus = false;
unsigned long overridePressedSince = 0;
bool overrideHandled = false;

void showIdle() {
    if (!apiClient.isProvisioned()) {
        // Keep the misconfiguration on screen instead of looking "Ready".
        displayDriver.showAccessDenied(apiClient.provisioningError());
        return;
    }
    displayDriver.showIdleScreen(networkManager.getLocalIp(), networkManager.isConnected(), overrideLog.totalCount());
}

void handleManualOverride() {
    unsigned long seq = overrideLog.record(networkManager.getIsoTimestamp(), networkManager.isTimeSynced(),
                                           networkManager.isConnected());
    displayDriver.showAccessGranted("MANUAL OVERRIDE", seq ? "Logged #" + String(seq) : String("NOT LOGGED"), "--");
    feedbackController.triggerFeedback(FeedbackType::SUCCESS_GRANTED);
    relayController.unlockGate(GATE_UNLOCK_DURATION_MS);
}

// GPIO 34 has no internal pull-up, so a single LOW sample is not trusted:
// the button must read LOW continuously for OVERRIDE_HOLD_MS, once per press.
void pollOverrideButton() {
    if (digitalRead(PIN_BUTTON_OVERRIDE) != LOW) {
        overridePressedSince = 0;
        overrideHandled = false;
        return;
    }
    if (overridePressedSince == 0) {
        overridePressedSince = millis();
        return;
    }
    if (!overrideHandled && millis() - overridePressedSince >= OVERRIDE_HOLD_MS) {
        overrideHandled = true;
        handleManualOverride();
    }
}

void denyScan(const char* message) {
    displayDriver.showAccessDenied(message);
    feedbackController.triggerFeedback(FeedbackType::ACCESS_DENIED);
}

void processIncomingScan(const String& rawData) {
    displayDriver.showScanningScreen();

    // 1. Only backend-issued "HH1:<64 hex>" passes are considered at all.
    ScanResult parsed = qrScanner.parsePayload(rawData);
    if (!parsed.isValid) {
        Serial.printf("[CORE] Rejected scan: %s\n", parsed.errorReason.c_str());
        denyScan("INVALID QR CODE");
        return;
    }
    String redacted = QrScanner::redact(parsed.qrPayload);

    // 2. Local duplicate suppression (FR014). The gate does not reopen for a
    //    repeat; the backend applies its own authoritative cooldown.
    unsigned long remainingCooldown = 0;
    if (qrScanner.isCooldownActive(parsed.qrPayload, remainingCooldown)) {
        Serial.printf("[CORE] Duplicate scan %s ignored (%lu s)\n", redacted.c_str(), remainingCooldown);
        displayDriver.showCooldownScreen(remainingCooldown);
        feedbackController.triggerFeedback(FeedbackType::SCAN_COOLDOWN);
        return;
    }
    qrScanner.registerScan(parsed.qrPayload);

    // 3. Fail closed: without a confirmed backend decision the gate stays locked.
    //    Offline scans are not queued, because a denied student did not move
    //    and replaying the scan later would record a movement that never happened.
    if (!apiClient.isProvisioned()) {
        denyScan("GATE NOT PROVISIONED");
        return;
    }
    if (!networkManager.isConnected()) {
        Serial.println("[CORE] Offline: gate stays locked.");
        displayDriver.showOfflineScreen();
        feedbackController.triggerFeedback(FeedbackType::SYSTEM_OFFLINE);
        return;
    }

    displayDriver.showVerifyingScreen(redacted);
    ScanResponse response = apiClient.submitScan(parsed.qrPayload);

    if (response.decision == gate::Decision::UNLOCK) {
        Serial.printf("[CORE] Access granted for %s (%s)\n", redacted.c_str(), response.outcome.c_str());
        // Restricted-hours movements are recorded as incidents by the backend.
        String note = response.accessResult == "unapproved" ? String("LATE - REPORTED") : String("WELCOME");
        displayDriver.showAccessGranted(note, redacted, response.outcome);
        feedbackController.triggerFeedback(FeedbackType::SUCCESS_GRANTED);
        relayController.unlockGate(GATE_UNLOCK_DURATION_MS);
        return;
    }

    Serial.printf("[CORE] Access denied for %s: %s\n", redacted.c_str(), gate::denialMessage(response.decision));
    if (response.decision == gate::Decision::DENY_UNAVAILABLE && response.httpCode <= 0) {
        displayDriver.showOfflineScreen();
        feedbackController.triggerFeedback(FeedbackType::SYSTEM_OFFLINE);
        return;
    }
    denyScan(gate::denialMessage(response.decision));
}

// Maintenance command over USB serial: "overrides" prints the override log.
bool handleSerialCommand(const String& line) {
    if (line == "overrides") {
        overrideLog.dump(Serial);
        return true;
    }
    return false;
}

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("\n=======================================================");
    Serial.println("  HOSTELHIVE IoT ATTENDANCE NODE FIRMWARE");
    Serial.printf("  Version: %s | Node: %s | Mode: %s\n", HOSTELHIVE_FIRMWARE_VERSION, NODE_ID, GATE_DIRECTION);
    Serial.printf("  Built: %s %s\n", __DATE__, __TIME__);
    Serial.println("=======================================================");

    // Relay first, so the gate is locked before anything else can fail.
    relayController.init();
    feedbackController.init();
    displayDriver.init();
    qrScanner.init();
    overrideLog.init();

    // Requires an external pull-up on GPIO 34 (see config.h).
    pinMode(PIN_BUTTON_OVERRIDE, INPUT);

    networkManager.init(WIFI_SSID, WIFI_PASSWORD);
    if (networkManager.isConnected()) {
        networkManager.initTimeSync(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
    }
    if (apiClient.init(API_BASE_URL, API_ROOT_CA, DEVICE_CREDENTIAL, ALLOW_INSECURE_HTTP != 0) &&
        networkManager.isConnected()) {
        unsigned long checkStart = millis();
        int check = apiClient.checkDevice();
        Serial.printf("[SETUP] Backend device check: HTTP %d%s after %lu ms (timeout %d ms)\n", check,
                      check == 200 ? " (authenticated)" : "", millis() - checkStart, HTTP_REQUEST_TIMEOUT_MS);
    }

    Serial.println("[SETUP] Initialization completed. Ready for scans.\n");
    showIdle();
}

void loop() {
    // 1. Subsystem background updates
    feedbackController.update();
    relayController.update();
    displayDriver.update();
    networkManager.update();

    // 2. Manual override button (held, debounced, logged)
    pollOverrideButton();

    // 3. Scan input detection
    if (qrScanner.hasData()) {
        String rawData = qrScanner.readRawPayload();
        if (rawData.length() > 0 && !handleSerialCommand(rawData)) {
            processIncomingScan(rawData);
        }
    }
#if !SIMULATION_MODE
    // In production the USB console is only used for maintenance commands.
    if (Serial.available() > 0) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        handleSerialCommand(command);
    }
#endif

    // 4. Periodic UI status refresh when idle
    bool currentWifi = networkManager.isConnected();
    if (displayDriver.getCurrentState() == ScreenState::IDLE) {
        if (millis() - lastUiRefresh >= 5000 || currentWifi != lastWifiStatus) {
            lastUiRefresh = millis();
            lastWifiStatus = currentWifi;
            showIdle();
        }
    }
}
