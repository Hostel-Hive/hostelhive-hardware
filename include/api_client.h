#pragma once
#ifndef HOSTELHIVE_API_CLIENT_H
#define HOSTELHIVE_API_CLIENT_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

struct ScanResponse {
    bool success;
    bool accessGranted;
    String studentName;
    String roomNumber;
    String message;
    int httpCode;
};

class ApiClient {
public:
    ApiClient();
    void init(const String& serverHost, const String& endpoint);
    ScanResponse dispatchScan(const String& studentId, const String& nodeId, const String& direction, const String& timestampIso);

private:
    String serverUrl;
};

extern ApiClient apiClient;

#endif // HOSTELHIVE_API_CLIENT_H
