#pragma once
#ifndef HOSTELHIVE_QR_SCANNER_H
#define HOSTELHIVE_QR_SCANNER_H

#include <Arduino.h>

#define MAX_COOLDOWN_ENTRIES 16

struct ScanResult {
    bool isValid;
    String studentId;
    String rawToken;
    String errorReason;
};

struct CooldownEntry {
    String studentId;
    unsigned long timestampMillis;
};

class QrScanner {
public:
    QrScanner();
    void init();
    bool hasData();
    String readRawPayload();
    ScanResult parsePayload(const String& rawData);

    // FR014: Duplicate scan prevention
    bool isCooldownActive(const String& studentId, unsigned long& remainingSecondsOut);
    void registerScan(const String& studentId);

protected:
    HardwareSerial* scannerSerial;
    CooldownEntry cooldownBuffer[MAX_COOLDOWN_ENTRIES];
    int cooldownCount;
};

extern QrScanner qrScanner;

#endif // HOSTELHIVE_QR_SCANNER_H
