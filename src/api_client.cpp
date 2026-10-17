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

        // Parse JSON response from Go backend
        StaticJsonDocument<512> responseDoc;
        DeserializationError error = deserializeJson(responseDoc, payload);

        if (!error) {
            resp.accessGranted = responseDoc["access"] | false;
            resp.studentName = responseDoc["student_name"] | "Student";
            resp.roomNumber = responseDoc["room_number"] | "--";
            resp.message = responseDoc["message"] | "";
        } else {
            // Non-JSON response fallback
            resp.accessGranted = (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED);
            resp.studentName = studentId;
            resp.message = "HTTP " + String(httpCode);
        }

        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED) {
            resp.success = true;
        } else if (httpCode == 403) {
            resp.success = false;
            resp.accessGranted = false;
            if (resp.message.length() == 0) resp.message = "Pass Revoked / Curfew";
        } else if (httpCode == 401) {
            resp.success = false;
            resp.accessGranted = false;
            resp.message = "Unauthorized Device Key";
        } else if (httpCode == 429) {
            resp.success = false;
            resp.accessGranted = false;
            resp.message = "Rate Limited Cooldown";
        } else {
            resp.success = false;
            resp.accessGranted = false;
            resp.message = "Server Error (" + String(httpCode) + ")";
        }
    } else {
        Serial.printf("[API] HTTP POST connection failed, error: %s\n", http.errorToString(httpCode).c_str());
        resp.success = false;
        resp.accessGranted = false;
        resp.message = "Network Timeout / Unreachable";
    }

    http.end();
    return resp;
}
