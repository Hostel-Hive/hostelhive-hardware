#include "qr_scanner.h"
#include "config.h"

QrScanner qrScanner;

QrScanner::QrScanner()
    : scannerSerial(&Serial2), cooldownCount(0) {
    for (int i = 0; i < MAX_COOLDOWN_ENTRIES; i++) {
        cooldownBuffer[i].studentId = "";
        cooldownBuffer[i].timestampMillis = 0;
    }
}

void QrScanner::init() {
    Serial2.begin(SCANNER_BAUD_RATE, SERIAL_8N1, PIN_SCANNER_RX, PIN_SCANNER_TX);
    Serial.println("[SCANNER] QR Scanner UART initialized on GPIO 16 (RX) / 17 (TX).");
#if SIMULATION_MODE
    Serial.println("[SCANNER] Simulation Mode Active: Also accepting QR input via USB Serial console.");
#endif
}

bool QrScanner::hasData() {
    bool hardwareAvailable = Serial2.available() > 0;
#if SIMULATION_MODE
    bool consoleAvailable = Serial.available() > 0;
    return hardwareAvailable || consoleAvailable;
#else
    return hardwareAvailable;
#endif
}

String QrScanner::readRawPayload() {
    String payload = "";

    if (Serial2.available() > 0) {
        payload = Serial2.readStringUntil('\n');
    }
#if SIMULATION_MODE
    else if (Serial.available() > 0) {
        payload = Serial.readStringUntil('\n');
    }
#endif

    payload.trim();
    return payload;
}

ScanResult QrScanner::parsePayload(const String& rawData) {
    ScanResult result;
    result.isValid = false;
    result.rawToken = rawData;
    result.studentId = "";
    result.errorReason = "";

    if (rawData.length() == 0) {
        result.errorReason = "Empty QR payload";
        return result;
    }

    String sanitized = rawData;
    sanitized.replace("\r", "");
    sanitized.replace("\n", "");
    sanitized.trim();

    if (sanitized.length() < 4) {
        result.errorReason = "Payload too short";
        return result;
    }

    if (sanitized.startsWith("HH:") || sanitized.startsWith("HH-")) {
        result.studentId = sanitized;
        result.isValid = true;
    } else if (sanitized.indexOf("STU") >= 0 || sanitized.indexOf("stu") >= 0) {
        result.studentId = sanitized;
        result.isValid = true;
    } else {
        result.studentId = sanitized;
        result.isValid = true;
    }

    return result;
}

bool QrScanner::isCooldownActive(const String& studentId, unsigned long& remainingSecondsOut) {
    unsigned long now = millis();

    for (int i = 0; i < cooldownCount; i++) {
        if (cooldownBuffer[i].studentId == studentId) {
            unsigned long elapsed = now - cooldownBuffer[i].timestampMillis;
            if (elapsed < SCAN_COOLDOWN_MS) {
                remainingSecondsOut = (SCAN_COOLDOWN_MS - elapsed + 999) / 1000;
                return true;
            }
        }
    }

    remainingSecondsOut = 0;
    return false;
}

void QrScanner::registerScan(const String& studentId) {
    unsigned long now = millis();

    // Check if entry exists and update
    for (int i = 0; i < cooldownCount; i++) {
        if (cooldownBuffer[i].studentId == studentId) {
            cooldownBuffer[i].timestampMillis = now;
            return;
        }
    }

    // Add new entry or replace oldest
    if (cooldownCount < MAX_COOLDOWN_ENTRIES) {
        cooldownBuffer[cooldownCount].studentId = studentId;
        cooldownBuffer[cooldownCount].timestampMillis = now;
        cooldownCount++;
    } else {
        // Find oldest entry
        int oldestIndex = 0;
        unsigned long oldestTime = cooldownBuffer[0].timestampMillis;
        for (int i = 1; i < MAX_COOLDOWN_ENTRIES; i++) {
            if (cooldownBuffer[i].timestampMillis < oldestTime) {
                oldestTime = cooldownBuffer[i].timestampMillis;
                oldestIndex = i;
            }
        }
        cooldownBuffer[oldestIndex].studentId = studentId;
        cooldownBuffer[oldestIndex].timestampMillis = now;
    }
}
