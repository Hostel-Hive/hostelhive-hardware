#include "qr_scanner.h"
#include "config.h"
#include "gate_policy.h"

QrScanner qrScanner;

QrScanner::QrScanner()
    : scannerSerial(&Serial2), cooldownCount(0) {
    for (int i = 0; i < MAX_COOLDOWN_ENTRIES; i++) {
        cooldownBuffer[i].qrPayload = "";
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

// Reads one line, capped at MAX_SCANNER_LINE_LENGTH so a malicious or faulty
// scanner cannot exhaust heap. Over-long lines are drained and returned empty.
static String readBoundedLine(Stream& in) {
    String line;
    bool overflow = false;
    unsigned long deadline = millis() + 200;
    while (millis() < deadline) {
        if (in.available() <= 0) {
            delay(1);
            continue;
        }
        char c = (char)in.read();
        if (c == '\n') break;
        if (line.length() >= MAX_SCANNER_LINE_LENGTH) {
            overflow = true;
            continue;
        }
        line += c;
    }
    if (overflow) return String("");
    line.trim();
    return line;
}

String QrScanner::readRawPayload() {
    if (Serial2.available() > 0) {
        return readBoundedLine(Serial2);
    }
#if SIMULATION_MODE
    if (Serial.available() > 0) {
        return readBoundedLine(Serial);
    }
#endif
    return String("");
}

ScanResult QrScanner::parsePayload(const String& rawData) {
    ScanResult result;
    result.isValid = false;
    result.qrPayload = "";
    result.errorReason = "";

    String sanitized = rawData;
    sanitized.trim();

    if (sanitized.length() == 0) {
        result.errorReason = "Empty QR payload";
        return result;
    }

    // Only the backend-issued format is accepted; student IDs, URLs or any
    // other barcode are rejected locally and never reach the gate relay.
    if (!gate::isValidQrPayload(sanitized.c_str(), sanitized.length())) {
        result.errorReason = "Not a HostelHive pass";
        return result;
    }

    result.qrPayload = sanitized;
    result.isValid = true;
    return result;
}

String QrScanner::redact(const String& qrPayload) {
    if (qrPayload.length() <= 8) return String("****");
    return qrPayload.substring(0, 8) + "...";
}

bool QrScanner::isCooldownActive(const String& qrPayload, unsigned long& remainingSecondsOut) {
    unsigned long now = millis();

    for (int i = 0; i < cooldownCount; i++) {
        if (cooldownBuffer[i].qrPayload == qrPayload) {
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

void QrScanner::registerScan(const String& qrPayload) {
    unsigned long now = millis();

    // Check if entry exists and update
    for (int i = 0; i < cooldownCount; i++) {
        if (cooldownBuffer[i].qrPayload == qrPayload) {
            cooldownBuffer[i].timestampMillis = now;
            return;
        }
    }

    // Add new entry or replace oldest
    if (cooldownCount < MAX_COOLDOWN_ENTRIES) {
        cooldownBuffer[cooldownCount].qrPayload = qrPayload;
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
        cooldownBuffer[oldestIndex].qrPayload = qrPayload;
        cooldownBuffer[oldestIndex].timestampMillis = now;
    }
}
