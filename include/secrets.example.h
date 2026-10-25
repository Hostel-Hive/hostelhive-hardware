#pragma once
#ifndef HOSTELHIVE_SECRETS_H
#define HOSTELHIVE_SECRETS_H

// ==========================================
// Network & Security Configuration Template
// Copy this file to include/secrets.h (git-ignored) and edit every value.
// The firmware refuses to build without include/secrets.h, and refuses to
// open the gate while DEVICE_CREDENTIAL is not a real issued credential.
// ==========================================

// Wi-Fi Credentials
// In Wokwi simulator, SSID is "Wokwi-GUEST" with empty password.
#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASSWORD ""

// HostelHive backend base URL. Must be https:// in production.
#define API_BASE_URL "https://api.example.com"

// PEM root CA that signs the backend's TLS certificate (for example ISRG Root X1
// for Let's Encrypt). The connection is refused if the server chain does not
// verify against it.
#define API_ROOT_CA \
    "-----BEGIN CERTIFICATE-----\n" \
    "REPLACE_WITH_ROOT_CA_PEM\n" \
    "-----END CERTIFICATE-----\n"

// Scanner credential returned once by POST /api/v1/scanner-devices (or
// rotate-credential), format: hhd1.<device-uuid>.<43-char-secret>.
// Rotate it from the admin panel if this device or file is ever exposed.
#define DEVICE_CREDENTIAL "REPLACE_WITH_ISSUED_SCANNER_CREDENTIAL"

// Simulation only: allow plain http:// to a local backend. Ignored (forced off)
// unless SIMULATION_MODE is 1. Never enable on a physical gate.
#define ALLOW_INSECURE_HTTP 0

// NTP Server Settings
#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET_SEC (5 * 3600 + 30 * 60)  // Sri Lanka Standard Time (UTC+05:30)
#define DAYLIGHT_OFFSET_SEC 0

#endif // HOSTELHIVE_SECRETS_H
