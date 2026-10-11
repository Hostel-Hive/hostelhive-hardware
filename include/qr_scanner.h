#pragma once
#ifndef HOSTELHIVE_QR_SCANNER_H
#define HOSTELHIVE_QR_SCANNER_H

#include <Arduino.h>

class QrScanner {
public:
    QrScanner();
    void init();
    bool hasData();
    String readRawPayload();

protected:
    HardwareSerial* scannerSerial;
};

extern QrScanner qrScanner;

#endif // HOSTELHIVE_QR_SCANNER_H
