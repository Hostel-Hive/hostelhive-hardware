#include "qr_scanner.h"
#include "config.h"

QrScanner qrScanner;

QrScanner::QrScanner()
    : scannerSerial(&Serial2) {}

void QrScanner::init() {
    // Initialize HardwareSerial2 on RX2=GPIO16, TX2=GPIO17
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
