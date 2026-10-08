#pragma once
#ifndef HOSTELHIVE_CONFIG_H
#define HOSTELHIVE_CONFIG_H

#include <Arduino.h>

// ==========================================
// Operational Mode & Academic Configuration
// ==========================================
#define HOSTELHIVE_FIRMWARE_VERSION "1.0.0"
#define SIMULATION_MODE 1  // 1 = Wokwi simulation / Testbed, 0 = Physical production

// Node Identification (Configurable per gate deployment)
#define NODE_ID "GATE_ENTRY_01"   // e.g., GATE_ENTRY_01 or GATE_EXIT_01
#define GATE_DIRECTION "IN"        // "IN" for entry gate, "OUT" for exit gate

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
#define PIN_BUTTON_OVERRIDE 34

// ==========================================
// Timing & Business Rule Constraints (SRS)
// ==========================================
// FR014: Duplicate scan cooldown period (in milliseconds)
#define SCAN_COOLDOWN_MS 15000

// Network & API Timeouts
#define HTTP_REQUEST_TIMEOUT_MS 5000
#define WIFI_RECONNECT_INTERVAL_MS 10000
#define NTP_SYNC_INTERVAL_MS 3600000

// Offline Queue Constraints (FR015)
#define MAX_OFFLINE_CACHE_ENTRIES 50
#define QUEUE_STORAGE_PATH "/attendance_queue.json"

#endif // HOSTELHIVE_CONFIG_H
