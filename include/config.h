#pragma once
#ifndef HOSTELHIVE_CONFIG_H
#define HOSTELHIVE_CONFIG_H

#include <Arduino.h>

// ==========================================
// Operational Mode & Academic Configuration
// ==========================================
#define HOSTELHIVE_FIRMWARE_VERSION "1.1.0"
// 1 = Wokwi simulation / testbed (QR input also accepted over USB serial),
// 0 = physical production. Set per environment in platformio.ini.
#ifndef SIMULATION_MODE
#define SIMULATION_MODE 0
#endif

// Node Identification (Configurable per gate deployment)
#define NODE_ID "GATE_ENTRY_01"   // e.g., GATE_ENTRY_01 or GATE_EXIT_01
// Display only: the backend decides direction from the admin-configured
// scanner device (PUT /api/v1/scanner-devices/{deviceID}/direction).
#define GATE_DIRECTION "IN"

// ==========================================
// Pinout Definitions (ESP32-WROOM-32)
// ==========================================
// I2C Pins for SSD1306 OLED (128x64)
#define PIN_OLED_SDA 21
#define PIN_OLED_SCL 22
#define OLED_SCREEN_WIDTH 128
#define OLED_SCREEN_HEIGHT 64
#define OLED_RESET_PIN -1

// Visual Status Indicators
#define PIN_LED_GREEN 18
#define PIN_LED_RED 19

// Auditory Feedback (Piezo Buzzer)
#define PIN_BUZZER 23
#define BUZZER_PWM_CHANNEL 0

// Gate Actuator (Relay / Solenoid Lock)
#define PIN_RELAY_LOCK 27
#define RELAY_ACTIVE_LEVEL HIGH
#define GATE_UNLOCK_DURATION_MS 3000  // Solenoid held open for 3 seconds

// QR Scanner Serial Interface (UART2)
#define PIN_SCANNER_RX 16
#define PIN_SCANNER_TX 17
#define SCANNER_BAUD_RATE 9600

// Manual Trigger / Override Button
// GPIO 34 is input-only and has NO internal pull-up: wire an external 10k
// pull-up to 3.3V, otherwise the floating pin reads LOW and opens the gate.
#define PIN_BUTTON_OVERRIDE 34
// The button must be held continuously this long to unlock (noise/bounce guard).
#define OVERRIDE_HOLD_MS 1000

// ==========================================
// Timing & Business Rule Constraints (SRS)
// ==========================================
// FR014: Duplicate scan cooldown period (in milliseconds)
#define SCAN_COOLDOWN_MS 15000

// Network & API Timeouts
#if SIMULATION_MODE
// Wokwi's network gateway adds seconds of latency per TLS handshake round trip;
// 5 s makes the handshake recv time out (mbedTLS -0x50).
#define HTTP_REQUEST_TIMEOUT_MS 20000
#else
#define HTTP_REQUEST_TIMEOUT_MS 5000
#endif
#define WIFI_RECONNECT_INTERVAL_MS 10000
#define NTP_SYNC_INTERVAL_MS 3600000

// Backend scanner API (hostelhive-backend docs/attendance-scans.md)
#define API_PATH_DEVICE_SCANS "/api/v1/device/scans"
#define API_PATH_DEVICE_CHECK "/api/v1/device/check"

// Local tamper-evident record of manual overrides (L3 in SECURITY_AUDIT.md)
#define OVERRIDE_LOG_PATH "/override_log.jsonl"
#define OVERRIDE_LOG_MAX_BYTES 16384

#endif // HOSTELHIVE_CONFIG_H
