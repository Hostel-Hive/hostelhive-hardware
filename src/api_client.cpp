#include "api_client.h"
#include "config.h"

ApiClient apiClient;

ApiClient::ApiClient()
    : serverUrl(""), apiKey("") {}

void ApiClient::init(const String& serverHost, const String& endpoint, const String& deviceApiKey) {
    serverUrl = serverHost + endpoint;
    apiKey = deviceApiKey;
    Serial.print("[API] Initialized endpoint target: ");
    Serial.println(serverUrl);
    Serial.println("[API] Hardware pre-shared key provisioned (SDS 6.3).");
}

ScanResponse ApiClient::dispatchScan(const String& studentId, const String& nodeId, const String& direction, const String& timestampIso, bool isOfflineBatch) {
    ScanResponse resp;
    resp.success = false;
    resp.accessGranted = false;
    resp.studentName = "";
    resp.roomNumber = "";
    resp.message = "Initial dispatch";
    resp.httpCode = 0;

    HTTPClient http;
    http.begin(serverUrl);
    http.setTimeout(HTTP_REQUEST_TIMEOUT_MS);
    http.addHeader("Content-Type", "application/json");

    // SDS 6.3 Security: Provision device identification & authentication headers
    http.addHeader("X-Device-API-Key", apiKey);
    http.addHeader("X-Node-ID", nodeId);

    StaticJsonDocument<256> doc;
    doc["student_id"] = studentId;
    doc["node_id"] = nodeId;
    doc["direction"] = direction;
    doc["timestamp"] = timestampIso;
    doc["offline_synced"] = isOfflineBatch;

    String requestBody;
    serializeJson(doc, requestBody);

    Serial.print("[API] Sending POST payload: ");
    Serial.println(requestBody);

    int httpCode = http.POST(requestBody);
    resp.httpCode = httpCode;

    if (httpCode > 0) {
        String payload = http.getString();
        Serial.printf("[API] Response code %d: %s\n", httpCode, payload.c_str());

        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED) {
            resp.success = true;
            resp.accessGranted = true;
            resp.message = "Scan recorded successfully";
        } else {
            resp.success = false;
            resp.accessGranted = false;
            resp.message = "Server rejected request";
        }
    } else {
        Serial.printf("[API] HTTP POST failed, error: %s\n", http.errorToString(httpCode).c_str());
        resp.message = "Network connection failed";
    }

    http.end();
    return resp;
}
