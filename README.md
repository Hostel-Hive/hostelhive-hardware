# HostelHive IoT Gate Attendance & Access Control Node

[![Firmware CI](https://github.com/hostelhive/hostelhive-hardware/actions/workflows/firmware-ci.yml/badge.svg)](https://github.com/hostelhive/hostelhive-hardware/actions)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-ESP32-orange.svg)](https://platformio.org/)
[![Simulation](https://img.shields.io/badge/Wokwi-Interactive%20Simulation-blue.svg)](https://wokwi.com/)

> **Academic Project:** SENG 34213 – System Development Project  
> **Institution:** Software Engineering Teaching Unit, Faculty of Science, University of Kelaniya  
> **Course Year:** Level 3  

---

## 1. Project Overview & Architectural Scope

The **HostelHive IoT Node** is an automated gate-mounted edge device engineered on the **ESP32-WROOM-32** architecture. Stationed at hostel entrance and exit gates, it scans HostelHive student QR passes, verifies each scan with the central Go backend over HTTPS, controls solenoid gate strikes, and provides visual (SSD1306 OLED) and auditory feedback. The gate **fails closed**: it opens only when the backend confirms the scan.

### Traceability to SRS Requirements & SDS Design
* **FR011:** Entry and exit scans are captured strictly via physical/simulated ESP32 hardware endpoints.
* **FR012:** Precise ISO 8601 timestamps (`YYYY-MM-DDTHH:MM:SS+05:30`) synchronized over SNTP (`pool.ntp.org`).
* **FR013:** Directional movement (`IN` vs `OUT`) toggled automatically based on node identity (`GATE_ENTRY_01` / `GATE_EXIT_01`).
* **FR014:** Duplicate scan prevention using an active 15-second cooldown hash buffer per student ID.
* **FR015 (revised for security):** When Wi-Fi or the backend is unreachable, the gate stays locked and shows "OFFLINE - LOCKED / Contact security". Scans are not queued for later, because a denied student did not move and replaying the scan would record a movement that never happened. Staff use the logged manual override.
* **SDS Section 6.3:** Each gate authenticates with its own backend-issued scanner credential (`Authorization: Bearer hhd1.<device-uuid>.<secret>`) over TLS that verifies a pinned root CA.

### Gate security model
* Only backend-issued passes (`HH1:` + 64 lowercase hex characters) are accepted. Anything else is rejected on the device.
* The relay opens only on HTTP 200 with `outcome` `recorded`, `cooldown`, `already_in` or `already_out`. Timeouts, no network, 4xx and 5xx all keep it locked (`include/gate_policy.h`).
* Restricted-hours movements (`access_result: unapproved`) still open the gate. The backend records them as incidents, and the display shows "LATE - REPORTED".
* The firmware does not build without `include/secrets.h`. It also refuses to operate (shows "GATE NOT PROVISIONED") unless `DEVICE_CREDENTIAL` is a well-formed issued credential, `API_BASE_URL` is `https://` and `API_ROOT_CA` is set.
* The production build (`esp32prod`) forces plain HTTP off and does not accept QR input from the USB console.
* The manual override needs a 1-second hold, and every use is appended to `/override_log.jsonl` on flash with a sequence number. Type `overrides` in the serial console to print the log.
* QR tokens and the device credential are never printed to the serial log. Only a redacted prefix such as `HH1:0123...` appears.
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
| **Manual Override** | `GPIO 34` | `INPUT` + **external 10k pull-up to 3.3V** | Active LOW | Staff button, hold 1 s to unlock (logged). GPIO 34 has no internal pull-up; without the resistor the floating pin can open the gate. |

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
* Paste a real student pass payload (`HH1:` + 64 hex characters, decoded from the QR returned by `GET /api/v1/me/qr` or `GET /api/v1/students/{id}/qr`) into the Serial Monitor and press **Enter**. Any other text is rejected as **INVALID QR CODE**.
* The OLED shows **AUTHENTICATING** while the scan is sent to the backend. The gate opens (Green LED, buzzer, relay) only if the backend confirms it.
* Repeating the same pass within 15 seconds triggers the **DUPLICATE SCAN (FR014)** screen and Red LED.
* Holding the virtual **Manual Override Button (Blue)** for 1 second opens the gate and records the override.
* To reach a local backend over plain HTTP in the simulator only, set `ALLOW_INSECURE_HTTP 1` and an `http://` `API_BASE_URL` in `secrets.h`. The `esp32prod` build ignores this setting.
* The simulator usually runs below real time, so its TLS handshake with the hosted (Render) backend is too slow and the server drops it (`start_ssl_client: -80`). Run `python simulation/https_relay.py` on your computer and set `API_BASE_URL "http://host.wokwi.internal:8080"` with `ALLOW_INSECURE_HTTP 1`. The relay forwards only `/api/v1/device/*` to the backend over verified HTTPS.

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
│   ├── gate_policy.h              # Fail-closed decision, QR/credential validation (unit tested)
│   ├── override_log.h             # LittleFS manual-override audit log header
│   └── api_client.h               # HTTPS REST API client header
├── src/
│   ├── main.cpp                   # Firmware core state machine
│   ├── display_driver.cpp         # OLED rendering implementations
│   ├── qr_scanner.cpp             # Serial QR reader and cooldown filter (FR014)
│   ├── buzzer_led.cpp             # Auditory and visual feedback drivers
│   ├── relay_control.cpp          # Solenoid pulse and timeout cutoff
│   ├── network_manager.cpp        # Wi-Fi link manager with exponential backoff
│   ├── override_log.cpp           # Append-only override record on flash
│   └── api_client.cpp             # TLS client for POST /api/v1/device/scans
├── test/
│   ├── test_qr_parser/
│   │   └── test_qr.cpp            # Unit test for strict HH1 pass validation
│   ├── test_cooldown/
│   │   └── test_cooldown.cpp      # Unit test for scan cooldown deduplication
│   └── test_gate_policy/
│       └── test_gate_policy.cpp   # Fail-closed decisions, credential and HTTPS checks
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
Edit `include/secrets.h` (it's git-ignored) and set:
* `WIFI_SSID` / `WIFI_PASSWORD`
* `API_BASE_URL`: the backend's `https://` origin, no path
* `API_ROOT_CA`: the PEM root CA of the backend's TLS certificate
* `DEVICE_CREDENTIAL`: the `hhd1.…` value shown once when an admin registers the scanner (`POST /api/v1/scanner-devices`). If it ever leaks, rotate it from the admin panel.

An admin must also set the scanner's direction (`PUT /api/v1/scanner-devices/{deviceID}/direction`). Until then every scan is denied as "GATE NOT CONFIGURED".

Use `pio run -e esp32prod` for a physical gate and `esp32dev` for the Wokwi simulation.

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
