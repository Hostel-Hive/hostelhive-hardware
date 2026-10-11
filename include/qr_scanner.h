#pragma once
#ifndef HOSTELHIVE_QR_SCANNER_H
#define HOSTELHIVE_QR_SCANNER_H

#include <Arduino.h>

struct ScanResult {
    bool isValid;
    String studentId;
    String rawToken;
    String errorReason;
};

class QrScanner {
public:
    QrScanner();
    void init();
    bool hasData();
    String readRawPayload();
    ScanResult parsePayload(const String& rawData);

protected:
    HardwareSerial* scannerSerial;
};

extern QrScanner qrScanner;

#endif // HOSTELHIVE_QR_SCANNER_H
