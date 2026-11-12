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

#if defined(ESP8266)
// NodeMCU gate: lock held open 5 s; the reader also pauses for these 5 s.
#define GATE_UNLOCK_DURATION_MS 5000
#else
#define GATE_UNLOCK_DURATION_MS 3000  // Solenoid held open for 3 seconds
#endif
// The override button must be held continuously this long to unlock (noise/bounce guard).
#define OVERRIDE_HOLD_MS 1000

#if defined(ESP8266)
// ==========================================
// Pinout Definitions (NodeMCU V3 / ESP-12E "ESP8266MOD")
// Scans come from an RC522 RFID reader; the card carries the student's HH1 pass.
// No OLED or external LEDs: status goes to the USB serial log and the onboard LED.
// ==========================================
#define HH_BOARD_NAME "NodeMCU V3 (ESP8266)"
#define HAS_RFID 1
#define HAS_QR_UART 0
#ifndef HAS_OLED
#define HAS_OLED 0
#endif

// RC522 on hardware SPI: SCK=D5 (GPIO14), MISO=D6 (GPIO12), MOSI=D7 (GPIO13)
#define PIN_RFID_SS 15   // D8
#define PIN_RFID_RST 5   // D1

// Onboard LED (GPIO2 / D4) is the only status light; it is lit by driving LOW.
#define PIN_LED_GREEN 2
#define PIN_LED_RED -1
#define LED_ACTIVE_LEVEL LOW

// Buzzer + to D2, - to GND. GPIO4 stays LOW at boot, so it is silent while resetting.
#define PIN_BUZZER 4     // D2
// 1 = active buzzer (has its own oscillator, e.g. TMB12A12 / TMB12A05): D2 is
//     simply switched on and off. 0 = passive piezo, driven with tone().
#ifndef BUZZER_ACTIVE
#define BUZZER_ACTIVE 1
#endif

// Relay IN on D0 (GPIO16). GPIO16 is HIGH during boot, so use an active-LOW
// relay module: the lock then stays shut while the board resets.
#define PIN_RELAY_LOCK 16  // D0
#define RELAY_ACTIVE_LEVEL LOW

// Manual override: the onboard FLASH button (GPIO0 / D3), which already has a
// pull-up. Do not hold it while powering on, or the board enters flash mode.
#define PIN_BUTTON_OVERRIDE 0
#define OVERRIDE_PIN_MODE INPUT_PULLUP

#else
// ==========================================
// Pinout Definitions (ESP32-WROOM-32)
// ==========================================
#define HH_BOARD_NAME "ESP32-WROOM-32"
#define HAS_RFID 0
#define HAS_QR_UART 1
#ifndef HAS_OLED
#define HAS_OLED 1
#endif

// I2C Pins for SSD1306 OLED (128x64)
#define PIN_OLED_SDA 21
#define PIN_OLED_SCL 22

// Visual Status Indicators
#define PIN_LED_GREEN 18
#define PIN_LED_RED 19
#define LED_ACTIVE_LEVEL HIGH

// Auditory Feedback (Piezo Buzzer)
#define PIN_BUZZER 23
#define BUZZER_PWM_CHANNEL 0
#ifndef BUZZER_ACTIVE
#define BUZZER_ACTIVE 0  // passive piezo (Wokwi buzzer), driven with tone()
#endif

// Gate Actuator (Relay / Solenoid Lock)
#define PIN_RELAY_LOCK 27
#define RELAY_ACTIVE_LEVEL HIGH

// QR Scanner Serial Interface (UART2)
#define PIN_SCANNER_RX 16
#define PIN_SCANNER_TX 17
#define SCANNER_BAUD_RATE 9600

// Manual Trigger / Override Button
// GPIO 34 is input-only and has NO internal pull-up: wire an external 10k
// pull-up to 3.3V, otherwise the floating pin reads LOW and opens the gate.
#define PIN_BUTTON_OVERRIDE 34
#define OVERRIDE_PIN_MODE INPUT
#endif

#define OLED_SCREEN_WIDTH 128
#define OLED_SCREEN_HEIGHT 64
#define OLED_RESET_PIN -1

// RFID card handling (NodeMCU build)
// How long "provision"/"wipe" waits for a card before giving up.
#define RFID_PROVISION_TIMEOUT_MS 20000
// The RC522 is re-checked (and re-initialised if it stopped answering) this often.
#define RFID_HEALTH_CHECK_MS 30000

// ==========================================
// Timing & Business Rule Constraints (SRS)
// ==========================================
#if HAS_RFID
// RFID gate: after a GRANTED scan every card is ignored while the gate is open,
// then the buzzer goes "beep-beep" for the next card. A rejected scan does not
// pause the reader: the next card can be tapped straight away.
#define SCAN_LOCKOUT_MS GATE_UNLOCK_DURATION_MS
// No separate per-pass cooldown: the lockout above covers repeat taps, and the
// backend applies its own cooldown to repeated movements.
#define SCAN_COOLDOWN_MS 0
#else
// FR014: Duplicate scan cooldown period (in milliseconds)
#define SCAN_COOLDOWN_MS 15000
#endif

// Network & API Timeouts
#if SIMULATION_MODE
// Wokwi's network gateway adds seconds of latency per TLS handshake round trip;
// 5 s makes the handshake recv time out (mbedTLS -0x50).
#define HTTP_REQUEST_TIMEOUT_MS 20000
#else
#define HTTP_REQUEST_TIMEOUT_MS 5000
#endif
#define WIFI_RECONNECT_INTERVAL_MS 10000
// How long setup() waits for Wi-Fi before carrying on offline. Phone hotspots
// often need 6-8 s; connecting here lets NTP and the backend check finish
// before the first card is tapped.
#define WIFI_CONNECT_WAIT_MS 12000
#define NTP_SYNC_INTERVAL_MS 3600000

// Backend scanner API (hostelhive-backend docs/attendance-scans.md)
#define API_PATH_DEVICE_SCANS "/api/v1/device/scans"
#define API_PATH_DEVICE_CHECK "/api/v1/device/check"

// Local tamper-evident record of manual overrides (L3 in SECURITY_AUDIT.md)
#define OVERRIDE_LOG_PATH "/override_log.jsonl"
#define OVERRIDE_LOG_MAX_BYTES 16384

#endif // HOSTELHIVE_CONFIG_H
