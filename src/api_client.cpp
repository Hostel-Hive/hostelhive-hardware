#include "api_client.h"
#include "config.h"
#include <esp_system.h>

ApiClient apiClient;

ApiClient::ApiClient()
    : baseUrl(""), rootCa(nullptr), authorization(""), insecure(false), provisioned(false), setupError("not initialized") {}

bool ApiClient::init(const char* url, const char* rootCaPem, const char* deviceCredential, bool allowInsecureHttp) {
    provisioned = false;
#if !SIMULATION_MODE
    // Plain HTTP would expose the device credential and student passes on Wi-Fi.
    allowInsecureHttp = false;
#endif
    insecure = allowInsecureHttp;

    if (!gate::isValidDeviceCredential(deviceCredential)) {
        setupError = "DEVICE_CREDENTIAL missing/invalid";
        Serial.println("[API] Gate locked: DEVICE_CREDENTIAL is not an issued hhd1 credential.");
        return false;
    }
    if (!gate::isHttpsUrl(url) && !insecure) {
        setupError = "API_BASE_URL must be https";
        Serial.println("[API] Gate locked: API_BASE_URL must use https://.");
        return false;
    }
    if (!insecure && (rootCaPem == nullptr || strstr(rootCaPem, "REPLACE_WITH") != nullptr ||
                      strstr(rootCaPem, "-----BEGIN CERTIFICATE-----") == nullptr)) {
        setupError = "API_ROOT_CA missing";
        Serial.println("[API] Gate locked: API_ROOT_CA is not a PEM certificate.");
        return false;
    }

    baseUrl = url;
    while (baseUrl.endsWith("/")) baseUrl.remove(baseUrl.length() - 1);
    rootCa = rootCaPem;
    authorization = String("Bearer ") + deviceCredential;
    if (!insecure) {
        tlsClient.setCACert(rootCa);
    } else {
        Serial.println("[API] WARNING: insecure HTTP enabled (simulation only).");
    }
    provisioned = true;
    setupError = "";
    // Never print the credential itself.
    Serial.printf("[API] Backend: %s (credential loaded)\n", baseUrl.c_str());
    return true;
}

bool ApiClient::beginRequest(HTTPClient& http, const char* path) {
    String url = baseUrl + path;
    http.setTimeout(HTTP_REQUEST_TIMEOUT_MS);
    http.setConnectTimeout(HTTP_REQUEST_TIMEOUT_MS);
    // Redirects could leak the Authorization header to another host.
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    http.setReuse(false);
    bool ok = insecure ? http.begin(plainClient, url) : http.begin(tlsClient, url);
    if (!ok) return false;
    http.addHeader("Authorization", authorization);
    return true;
}

int ApiClient::checkDevice() {
    if (!provisioned) return -1;
    HTTPClient http;
    if (!beginRequest(http, API_PATH_DEVICE_CHECK)) return -1;
    int code = http.GET();
    http.end();
    return code;
}

ScanResponse ApiClient::submitScan(const String& qrPayload) {
    ScanResponse resp;
    resp.decision = gate::Decision::DENY_UNAVAILABLE;
    resp.httpCode = 0;

    if (!provisioned || !gate::isValidQrPayload(qrPayload.c_str(), qrPayload.length())) {
        resp.decision = provisioned ? gate::Decision::DENY_INVALID_PASS : gate::Decision::DENY_DEVICE_UNAUTHORIZED;
        return resp;
    }

    // One event ID per physical scan. The backend treats a repeated ID with the
    // same QR as a retry and returns the original result, so one retry is safe.
    uint8_t random[16];
    esp_fill_random(random, sizeof(random));
    char eventId[37];
    gate::formatUuidV4(random, eventId);

    StaticJsonDocument<192> body;
    body["event_id"] = eventId;
    body["qr"] = qrPayload;
    String requestBody;
    serializeJson(body, requestBody);

    for (int attempt = 0; attempt < 2; attempt++) {
        HTTPClient http;
        if (!beginRequest(http, API_PATH_DEVICE_SCANS)) {
            resp.httpCode = -1;
            continue;
        }
        http.addHeader("Content-Type", "application/json");
        unsigned long started = millis();
        resp.httpCode = http.POST(requestBody);

        if (resp.httpCode > 0) {
            String payload = http.getSize() > 2048 ? String("") : http.getString();
            http.end();
            StaticJsonDocument<768> doc;
            if (!deserializeJson(doc, payload)) {
                resp.outcome = doc["outcome"] | "";
                resp.accessResult = doc["access_result"] | "";
                resp.errorCode = doc["error"] | "";
            }
            // An unparseable 200 has an empty outcome, which decide() denies.
            resp.decision = gate::decide(resp.httpCode, resp.outcome.length() ? resp.outcome.c_str() : nullptr,
                                         resp.errorCode.length() ? resp.errorCode.c_str() : nullptr);
            Serial.printf("[API] Scan %s -> HTTP %d outcome=%s access=%s error=%s\n", eventId, resp.httpCode,
                          resp.outcome.c_str(), resp.accessResult.c_str(), resp.errorCode.c_str());
            return resp;
        }

        Serial.printf("[API] Scan %s attempt %d failed after %lu ms: %s\n", eventId, attempt + 1, millis() - started,
                      HTTPClient::errorToString(resp.httpCode).c_str());
        http.end();
    }

    // Fail closed: no response means no unlock.
    resp.decision = gate::Decision::DENY_UNAVAILABLE;
    return resp;
}
