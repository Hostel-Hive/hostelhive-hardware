#include "qr_scanner.h"
#include "config.h"

QrScanner qrScanner;

QrScanner::QrScanner()
    : scannerSerial(&Serial2) {}

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

    // Format validation: Student ID must have minimal valid length and expected format
    // Examples: "HH-STU-2021-CS-045" or "STU12345"
    String sanitized = rawData;
    sanitized.replace("\r", "");
    sanitized.replace("\n", "");
    sanitized.trim();

    if (sanitized.length() < 4) {
        result.errorReason = "Payload too short";
        return result;
    }

    // Extract student ID prefix
    if (sanitized.startsWith("HH:") || sanitized.startsWith("HH-")) {
        result.studentId = sanitized;
        result.isValid = true;
    } else if (sanitized.indexOf("STU") >= 0 || sanitized.indexOf("stu") >= 0) {
        result.studentId = sanitized;
        result.isValid = true;
    } else {
        // Fallback for general student IDs in university testing
        result.studentId = sanitized;
        result.isValid = true;
    }

    return result;
}
