#pragma once
#ifndef HOSTELHIVE_GATE_POLICY_H
#define HOSTELHIVE_GATE_POLICY_H

// Security decisions for the gate, kept free of Arduino dependencies so the
// native unit tests exercise exactly the code that ships on the device.
//
// Contract with hostelhive-backend (POST /api/v1/device/scans):
//   Authorization: Bearer hhd1.<lowercase-uuid>.<43 base64url chars>
//   Body: {"event_id":"<uuid>","qr":"HH1:<64 lowercase hex>"}
//   200 + outcome recorded|cooldown|already_in|already_out => scan accepted.
//   Anything else (4xx, 5xx, timeout, no network) => gate stays locked.

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace gate {

static const char QR_PREFIX[] = "HH1:";
static const size_t QR_PREFIX_LEN = 4;
static const size_t QR_TOKEN_HEX_LEN = 64;
static const size_t QR_PAYLOAD_LEN = QR_PREFIX_LEN + QR_TOKEN_HEX_LEN;  // 68

static const char CREDENTIAL_PREFIX[] = "hhd1.";
static const size_t CREDENTIAL_PREFIX_LEN = 5;
static const size_t UUID_LEN = 36;
static const size_t CREDENTIAL_SECRET_LEN = 43;  // base64url(32 bytes), unpadded
static const size_t CREDENTIAL_LEN = CREDENTIAL_PREFIX_LEN + UUID_LEN + 1 + CREDENTIAL_SECRET_LEN;  // 85

inline bool isLowerHex(char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }

inline bool isBase64Url(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
}

// Exactly the payload the backend issues: "HH1:" + 64 lowercase hex characters.
// Leading/trailing whitespace from the scanner UART must be trimmed by the caller.
inline bool isValidQrPayload(const char* payload, size_t len) {
    if (payload == NULL || len != QR_PAYLOAD_LEN) return false;
    if (memcmp(payload, QR_PREFIX, QR_PREFIX_LEN) != 0) return false;
    for (size_t i = QR_PREFIX_LEN; i < len; i++) {
        if (!isLowerHex(payload[i])) return false;
    }
    return true;
}

// Canonical lowercase 8-4-4-4-12 UUID text.
inline bool isCanonicalUuid(const char* s, size_t len) {
    if (s == NULL || len != UUID_LEN) return false;
    for (size_t i = 0; i < len; i++) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (s[i] != '-') return false;
        } else if (!isLowerHex(s[i])) {
            return false;
        }
    }
    // The nil UUID is rejected by the backend as well.
    for (size_t i = 0; i < len; i++) {
        if (s[i] != '-' && s[i] != '0') return true;
    }
    return false;
}

// Shape check for the credential issued by POST /api/v1/scanner-devices.
inline bool isValidDeviceCredential(const char* credential) {
    if (credential == NULL) return false;
    size_t len = strlen(credential);
    if (len != CREDENTIAL_LEN) return false;
    if (memcmp(credential, CREDENTIAL_PREFIX, CREDENTIAL_PREFIX_LEN) != 0) return false;
    if (!isCanonicalUuid(credential + CREDENTIAL_PREFIX_LEN, UUID_LEN)) return false;
    if (credential[CREDENTIAL_PREFIX_LEN + UUID_LEN] != '.') return false;
    const char* secret = credential + CREDENTIAL_PREFIX_LEN + UUID_LEN + 1;
    for (size_t i = 0; i < CREDENTIAL_SECRET_LEN; i++) {
        if (!isBase64Url(secret[i])) return false;
    }
    // 32 bytes encode to 43 chars; the final char carries 2 padding bits that must be zero.
    const char last = secret[CREDENTIAL_SECRET_LEN - 1];
    return strchr("AEIMQUYcgkosw048", last) != NULL;
}

inline bool isHttpsUrl(const char* url) {
    return url != NULL && strncmp(url, "https://", 8) == 0 && strlen(url) > 8;
}

// RFC 4122 version 4 UUID from 16 random bytes. `out` must hold 37 chars.
inline void formatUuidV4(const uint8_t random[16], char out[37]) {
    static const char hex[] = "0123456789abcdef";
    uint8_t b[16];
    memcpy(b, random, 16);
    b[6] = (uint8_t)((b[6] & 0x0F) | 0x40);
    b[8] = (uint8_t)((b[8] & 0x3F) | 0x80);
    size_t o = 0;
    for (int i = 0; i < 16; i++) {
        if (i == 4 || i == 6 || i == 8 || i == 10) out[o++] = '-';
        out[o++] = hex[b[i] >> 4];
        out[o++] = hex[b[i] & 0x0F];
    }
    out[o] = '\0';
}

enum class Decision {
    UNLOCK,
    DENY_INVALID_PASS,       // 422 unknown/inactive/archived student QR
    DENY_DEVICE_UNAUTHORIZED,// 401 bad, rotated or deactivated device credential
    DENY_GATE_NOT_CONFIGURED,// 409 scanner_direction_required
    DENY_BAD_REQUEST,        // 400/409/413/415 malformed request
    DENY_UNAVAILABLE         // timeout, no network, 5xx, unparseable 200, anything else
};

// The gate opens only when the backend confirms it processed the scan for an
// active student. There is deliberately no branch that unlocks on failure.
inline Decision decide(int httpCode, const char* outcome) {
    if (httpCode == 200) {
        if (outcome == NULL) return Decision::DENY_UNAVAILABLE;
        if (strcmp(outcome, "recorded") == 0 || strcmp(outcome, "cooldown") == 0 ||
            strcmp(outcome, "already_in") == 0 || strcmp(outcome, "already_out") == 0) {
            return Decision::UNLOCK;
        }
        return Decision::DENY_UNAVAILABLE;
    }
    switch (httpCode) {
        case 422: return Decision::DENY_INVALID_PASS;
        case 401: return Decision::DENY_DEVICE_UNAUTHORIZED;
        case 409: return Decision::DENY_GATE_NOT_CONFIGURED;
        case 400:
        case 413:
        case 415: return Decision::DENY_BAD_REQUEST;
        default: return Decision::DENY_UNAVAILABLE;
    }
}

// 409 is shared by scanner_direction_required and event_id_conflict; the error
// code in the body tells them apart.
inline Decision decide(int httpCode, const char* outcome, const char* errorCode) {
    if (httpCode == 409 && errorCode != NULL && strcmp(errorCode, "scanner_direction_required") != 0) {
        return Decision::DENY_BAD_REQUEST;
    }
    return decide(httpCode, outcome);
}

inline const char* denialMessage(Decision d) {
    switch (d) {
        case Decision::DENY_INVALID_PASS: return "INVALID / INACTIVE PASS";
        case Decision::DENY_DEVICE_UNAUTHORIZED: return "GATE NOT AUTHORIZED";
        case Decision::DENY_GATE_NOT_CONFIGURED: return "GATE NOT CONFIGURED";
        case Decision::DENY_BAD_REQUEST: return "SCAN REJECTED";
        case Decision::DENY_UNAVAILABLE: return "SERVICE UNAVAILABLE";
        default: return "";
    }
}

}  // namespace gate

#endif  // HOSTELHIVE_GATE_POLICY_H
