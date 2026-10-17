# HostelHive IoT Gate Attendance & Access Control Node

[![Firmware CI](https://github.com/hostelhive/hostelhive-hardware/actions/workflows/firmware-ci.yml/badge.svg)](https://github.com/hostelhive/hostelhive-hardware/actions)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-ESP32-orange.svg)](https://platformio.org/)
[![Simulation](https://img.shields.io/badge/Wokwi-Interactive%20Simulation-blue.svg)](https://wokwi.com/)

> **Academic Project:** SENG 34213 – System Development Project  
> **Institution:** Software Engineering Teaching Unit, Faculty of Science, University of Kelaniya  
> **Course Year:** Level 3  

---

## 1. Project Overview & Architectural Scope

The **HostelHive IoT Node** is an automated gate-mounted edge device engineered on the **ESP32-WROOM-32** architecture. Stationed at hostel entrance and exit gates, it scans student university ID QR codes, authenticates credentials with the central Go backend via HTTPS REST APIs, controls solenoid gate strikes, provides visual (SSD1306 OLED) and auditory feedback, and ensures zero data loss during network outages through local flash buffering.

### Traceability to SRS Requirements & SDS Design
* **FR011:** Entry and exit scans are captured strictly via physical/simulated ESP32 hardware endpoints.
* **FR012:** Precise ISO 8601 timestamps (`YYYY-MM-DDTHH:MM:SS+05:30`) synchronized over SNTP (`pool.ntp.org`).
* **FR013:** Directional movement (`IN` vs `OUT`) toggled automatically based on node identity (`GATE_ENTRY_01` / `GATE_EXIT_01`).
* **FR014:** Duplicate scan prevention using an active 15-second cooldown hash buffer per student ID.
* **FR015:** High-availability offline caching via **LittleFS / SPIFFS** flash storage; buffered scans auto-flush sequentially upon Wi-Fi link restoration.
* **SDS Section 6.3:** Hardware source verification using node pre-shared authentication keys (`X-Device-API-Key`).
* **SDS Section 7.5.3:** PlatformIO CI automated build, flash partition limit validation, and unit testing.

---

## 2. Hardware Architecture & Pinout Specification

| Peripheral / Module | ESP32 GPIO Pin | Pin Mode | Logic Level | Functional Description |
|:---|:---|:---|:---|:---|
| **OLED Display (SDA)** | `GPIO 21` | I2C SDA | 3.3V | SSD1306 128x64 display data line |
| **OLED Display (SCL)** | `GPIO 22` | I2C SCL | 3.3V | SSD1306 128x64 display clock line |
| **Status LED: Green** | `GPIO 18` | `OUTPUT` | Active HIGH | Illuminates on access granted |
| **Status LED: Red** | `GPIO 19` | `OUTPUT` | Active HIGH | Illuminates on access denied / duplicate / offline |
| **Piezo Buzzer** | `GPIO 23` | `OUTPUT` (PWM) | Active HIGH | Auditory feedback (2400 Hz success chirp / 450 Hz alert buzz) |
| **Gate Lock Relay** | `GPIO 27` | `OUTPUT` | Active HIGH | Triggers 5V solenoid lock pulse (3000ms duration) |
| **QR Scanner (RX)** | `GPIO 16` (RX2) | `INPUT` | 3.3V UART | Reads raw QR payload stream from scanner module |
| **QR Scanner (TX)** | `GPIO 17` (TX2) | `OUTPUT` | 3.3V UART | Scanner configuration commands |
| **Manual Override** | `GPIO 34` | `INPUT_PULLUP`| Active LOW | Emergency staff button to unlock gate |

---

## 3. Interactive Wokwi Simulation (Hardware-Free Testing)

This codebase features complete, zero-hardware simulation support using **Wokwi**:

### Running the Simulation in VS Code:
1. Install the **Wokwi for VS Code** extension from the Extension Marketplace.
2. Compile the firmware using PlatformIO:
   ```bash
   pio run -e esp32dev
   ```
3. Open `simulation/diagram.json` in VS Code and press **F1 -> Wokwi: Start Simulator**.
4. The virtual ESP32 will boot, connect to `Wokwi-GUEST` virtual Wi-Fi, initialize the OLED screen, and display `SCAN ID`.

### Simulating QR Code Scans:
Because the firmware includes dual physical UART and USB Serial multiplexing:
* Type a test student ID in the Serial Monitor (e.g., `HH-STU-2021-CS-045`) and press **Enter**.
* The virtual OLED screen will immediately transition to **AUTHENTICATING**, test the cooldown buffer, dispatch to the backend, flash the **Green LED**, chirp the **Buzzer**, and activate the **Relay**.
* Entering the same ID within 15 seconds will trigger the **DUPLICATE SCAN (FR014)** warning screen and Red LED.
* Pressing the virtual **Manual Override Button (Blue)** will trigger an emergency gate pulse.

---

## 4. Repository Structure

```text
hostelhive-hardware/
├── .github/
│   └── workflows/
│       └── firmware-ci.yml        # GitHub Actions CI for PlatformIO compile & testing (SDS 7.5.3)
├── docs/
│   ├── circuit_diagram.svg        # Circuit schematic diagram
│   ├── bill_of_materials.md       # Component specifications & BOM
│   ├── pinout_mapping.md          # Complete GPIO pinout table
│   └── api_contract.md            # Hardware-to-Backend HTTP payload specification
├── include/
│   ├── config.h                   # Pin definitions, device IDs, and business rule constants
│   ├── secrets.example.h          # Wi-Fi credentials and API Key template
│   ├── display_driver.h           # SSD1306 OLED UI state machine header
│   ├── qr_scanner.h               # Hardware UART & console QR reader header
│   ├── buzzer_led.h               # LED indicator and buzzer tone patterns header
│   ├── relay_control.h            # Solenoid gate lock trigger header
│   ├── network_manager.h          # Wi-Fi reconnect and SNTP timestamp header
│   ├── offline_queue.h            # LittleFS flash storage queue header (FR015)
│   └── api_client.h               # HTTPS REST API client header
├── src/
│   ├── main.cpp                   # Firmware core state machine
│   ├── display_driver.cpp         # OLED rendering implementations
│   ├── qr_scanner.cpp             # Serial QR reader and cooldown filter (FR014)
│   ├── buzzer_led.cpp             # Auditory and visual feedback drivers
│   ├── relay_control.cpp          # Solenoid pulse and timeout cutoff
│   ├── network_manager.cpp        # Wi-Fi link manager with exponential backoff
│   ├── offline_queue.cpp          # LittleFS FIFO buffer and auto-flush routine
│   └── api_client.cpp             # JSON payload serialization and HTTPS POST dispatcher
├── test/
│   ├── test_qr_parser/
│   │   └── test_qr.cpp            # Unit test for QR format sanitization
│   ├── test_cooldown/
│   │   └── test_cooldown.cpp      # Unit test for scan cooldown deduplication
│   └── test_offline_queue/
│       └── test_queue.cpp         # Unit test for LittleFS JSON buffering
├── simulation/
│   ├── diagram.json               # Interactive Wokwi circuit simulation schematic
│   └── wokwi.toml                 # Wokwi simulation configuration
├── .gitignore                     # Git ignore rules for build cache and secrets
├── platformio.ini                 # PlatformIO project and build environment configuration
└── README.md                      # Hardware documentation and developer manual
```

---

## 5. Local Setup & Build Instructions

### Prerequisites
* [PlatformIO Core](https://platformio.org/install/cli) or [PlatformIO IDE for VS Code](https://platformio.org/install/ide)
* C++11 / C++14 toolchain

### Configuration
Copy the configuration template:
```bash
cp include/secrets.example.h include/secrets.h
```
Edit `include/secrets.h` to supply your Wi-Fi credentials, backend API server URL, and provisioned `DEVICE_API_KEY`.

### Compilation & Flash
```bash
# Compile firmware
pio run -e esp32dev

# Upload to physical ESP32 over USB
pio run -e esp32dev --target upload

# Open Serial Monitor
pio device monitor --speed 115200
```

### Running Automated Unit Tests
```bash
# Run standalone hardware & API logic test runner (Zero dependencies)
python test/run_tests.py

# Run PlatformIO Unity unit test suite
pio test -e native_test
```

---

## 6. License & Contributors
Developed for **HostelHive** as part of **SENG 34213 - System Development Project**.  
Maintained by **Hasinthaka Piyumal** (Hardware & IoT Subsystem Lead).
