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
#include "rfid_reader.h"

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
#if HAS_RFID
    if (!rfidReader.isReaderOk()) {
        displayDriver.showAccessDenied("RFID READER NOT RESPONDING");
        return;
    }
#endif
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

#if HAS_RFID
// One card per loop: a pass goes through the same path as a QR scan.
void pollRfid() {
    CardRead card = rfidReader.poll();
    switch (card.status) {
        case CardStatus::NONE:
            return;
        case CardStatus::PASS:
            Serial.printf("[RFID] Card %s carries pass %s\n", card.uid.c_str(), QrScanner::redact(card.payload).c_str());
            processIncomingScan(card.payload);
            return;
        case CardStatus::NOT_REGISTERED:
            denyScan("CARD NOT REGISTERED");
            return;
        case CardStatus::CORRUPT:
            denyScan("CARD DATA DAMAGED");
            return;
        case CardStatus::UNSUPPORTED:
            denyScan("UNSUPPORTED CARD");
            return;
        case CardStatus::READ_ERROR:
            denyScan("READ ERROR - TAP AGAIN");
            return;
        case CardStatus::PROVISION_OK:
            Serial.printf("[RFID] Pass written to card %s and verified.\n", card.uid.c_str());
            feedbackController.triggerFeedback(FeedbackType::SUCCESS_GRANTED);
            return;
        case CardStatus::WIPE_OK:
            Serial.printf("[RFID] HostelHive data erased from card %s.\n", card.uid.c_str());
            feedbackController.triggerFeedback(FeedbackType::SUCCESS_GRANTED);
            return;
        case CardStatus::PROVISION_FAILED:
            Serial.println("[RFID] Write failed; still waiting. Tap the card again, or send \"cancel\".");
            feedbackController.triggerFeedback(FeedbackType::ACCESS_DENIED);
            return;
        case CardStatus::PROVISION_TIMEOUT:
            Serial.println("[RFID] No card tapped in time; write cancelled.");
            feedbackController.triggerFeedback(FeedbackType::ERROR_ALERT);
            return;
    }
}
#endif

// Maintenance commands over USB serial:
//   overrides            print the manual-override log
//   provision HH1:<hex>  write a student's pass to the next tapped card (NodeMCU)
//   wipe                 erase HostelHive data from the next tapped card (NodeMCU)
//   cancel               abandon a pending provision/wipe
//   rfid                 RC522 status
bool handleSerialCommand(const String& line) {
    if (line == "overrides") {
        overrideLog.dump(Serial);
        return true;
    }
#if HAS_RFID
    if (line.startsWith("provision")) {
        String pass = line.substring(9);
        pass.trim();
        if (!rfidReader.armProvision(pass)) {
            Serial.println("[RFID] Usage: provision HH1:<64 lowercase hex> (the student's QR pass text).");
            return true;
        }
        Serial.printf("[RFID] Tap the card to write %s (%d s)...\n", QrScanner::redact(pass).c_str(),
                      RFID_PROVISION_TIMEOUT_MS / 1000);
        return true;
    }
    if (line == "wipe") {
        rfidReader.armWipe();
        Serial.printf("[RFID] Tap the card to erase its HostelHive data (%d s)...\n", RFID_PROVISION_TIMEOUT_MS / 1000);
        return true;
    }
    if (line == "cancel") {
        bool wasArmed = rfidReader.isArmed();
        rfidReader.cancel();
        Serial.println(wasArmed ? "[RFID] Pending card write cancelled." : "[RFID] Nothing to cancel.");
        return true;
    }
    if (line == "rfid") {
        Serial.printf("[RFID] RC522 %s (firmware 0x%02X)%s\n", rfidReader.isReaderOk() ? "OK" : "NOT RESPONDING",
                      rfidReader.firmwareVersion(), rfidReader.isArmed() ? ", waiting for a card to write" : "");
        return true;
    }
#endif
    return false;
}

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("\n=======================================================");
    Serial.println("  HOSTELHIVE IoT ATTENDANCE NODE FIRMWARE");
    Serial.printf("  Version: %s | Node: %s | Mode: %s\n", HOSTELHIVE_FIRMWARE_VERSION, NODE_ID, GATE_DIRECTION);
    Serial.printf("  Board: %s\n", HH_BOARD_NAME);
    Serial.printf("  Built: %s %s\n", __DATE__, __TIME__);
    Serial.println("=======================================================");

    // Relay first, so the gate is locked before anything else can fail.
    relayController.init();
    feedbackController.init();
    displayDriver.init();
    qrScanner.init();
#if HAS_RFID
    rfidReader.init();
#endif
    overrideLog.init();

    // ESP32: GPIO 34 needs an external pull-up. NodeMCU: onboard FLASH button (see config.h).
    pinMode(PIN_BUTTON_OVERRIDE, OVERRIDE_PIN_MODE);

    networkManager.init(WIFI_SSID, WIFI_PASSWORD);
    // Starts SNTP even when Wi-Fi comes up later (ESP8266 needs the clock for TLS).
    networkManager.initTimeSync(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
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
#if HAS_RFID
    pollRfid();
#endif
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
