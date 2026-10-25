#pragma once
#ifndef HOSTELHIVE_SECRETS_EXAMPLE_H
#define HOSTELHIVE_SECRETS_EXAMPLE_H

// ==========================================
// Network & Security Configuration Template
// Copy this file to include/secrets.h and edit credentials.
// ==========================================

// Wi-Fi Credentials
// In Wokwi simulator, SSID is "Wokwi-GUEST" with empty password.
#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASSWORD ""

// HostelHive Backend API Server Configuration
#define API_SERVER_HOST "http://192.168.1.100:8080"
#define API_ENDPOINT_ATTENDANCE "/api/v1/attendance/scan"

// Hardware Pre-Shared Key (SDS Section 6.3 - Hardware Security)
#define DEVICE_API_KEY "hh_dev_sec_gate_entry_987654321"

// NTP Server Settings
#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET_SEC (5 * 3600 + 30 * 60)  // Sri Lanka Standard Time (UTC+05:30)
#define DAYLIGHT_OFFSET_SEC 0

#endif // HOSTELHIVE_SECRETS_EXAMPLE_H
