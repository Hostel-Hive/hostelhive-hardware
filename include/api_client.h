#pragma once
#ifndef HOSTELHIVE_API_CLIENT_H
#define HOSTELHIVE_API_CLIENT_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "gate_policy.h"

struct ScanResponse {
    gate::Decision decision;
    int httpCode;          // <= 0 means the request never completed
    String outcome;        // recorded | cooldown | already_in | already_out
    String accessResult;   // approved | unapproved | not_required | not_evaluated
    String errorCode;      // backend {"error": "..."} on failure
};

class ApiClient {
public:
    ApiClient();
    // Returns false (and the gate stays locked) when the URL is not HTTPS, the
    // CA is missing, or the credential is not a well-formed issued credential.
    bool init(const char* baseUrl, const char* rootCaPem, const char* deviceCredential, bool allowInsecureHttp);
    bool isProvisioned() const { return provisioned; }
    const char* provisioningError() const { return setupError; }

    ScanResponse submitScan(const String& qrPayload);
    int checkDevice();  // GET /api/v1/device/check, returns HTTP status

private:
    bool beginRequest(HTTPClient& http, const char* path);

    String baseUrl;
    const char* rootCa;
    String authorization;
    bool insecure;
    bool provisioned;
    const char* setupError;
    WiFiClientSecure tlsClient;
    WiFiClient plainClient;
};

extern ApiClient apiClient;

#endif // HOSTELHIVE_API_CLIENT_H
