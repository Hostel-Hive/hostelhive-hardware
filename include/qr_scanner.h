#pragma once
#ifndef HOSTELHIVE_QR_SCANNER_H
#define HOSTELHIVE_QR_SCANNER_H

#include <Arduino.h>

#define MAX_COOLDOWN_ENTRIES 16
// Longest line read from the scanner UART; anything longer is discarded.
#define MAX_SCANNER_LINE_LENGTH 128

struct ScanResult {
    bool isValid;
    String qrPayload;  // "HH1:<64 hex>" exactly as issued by the backend
    String errorReason;
};

struct CooldownEntry {
    String qrPayload;
    unsigned long timestampMillis;
};

class QrScanner {
public:
    QrScanner();
    void init();
    bool hasData();
    String readRawPayload();
    ScanResult parsePayload(const String& rawData);

    // Short, non-secret form for logs and the display: "HH1:1a2b…".
    static String redact(const String& qrPayload);

    // FR014: Duplicate scan prevention
    bool isCooldownActive(const String& qrPayload, unsigned long& remainingSecondsOut);
    void registerScan(const String& qrPayload);

protected:
    HardwareSerial* scannerSerial;
    CooldownEntry cooldownBuffer[MAX_COOLDOWN_ENTRIES];
    int cooldownCount;
};

extern QrScanner qrScanner;

#endif // HOSTELHIVE_QR_SCANNER_H
