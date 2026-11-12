# Bill of Materials (BOM) & Component Specifications
**Project:** HostelHive IoT Attendance & Access Control Node  
**Academic Reference:** SENG 34213 - System Development Project  
**Design Reference:** SDS Section 7.2.2 (Table 11: On-Premises Hardware Requirements)

## 1. Hardware Overview
HostelHive employs an automated IoT gate scanning node stationed at hostel entrance and exit gates. The node operates both in a physical deployment and in an interactive virtual simulation environment (Wokwi).

| Component | Physical Model / Specification | Simulation Equivalent (Wokwi) | Purpose / Interface |
|:---|:---|:---|:---|
| **Microcontroller** | ESP32-WROOM-32 (30-pin / 38-pin DevKit V1) | `wokwi-esp32-devkit-v1` | Core processing, Wi-Fi networking, HTTPS requests, flash buffer storage |
| **Status Display** | 0.96" OLED Display (128x64, SSD1306) | `wokwi-ssd1306` | Real-time visual feedback: gate status, student name, errors, queue count (I2C) |
| **QR Code Scanner** | GM66 / Waveshare Barcode Scanner Module | Serial Stream / UART Console Mock | Scans student ID QR codes and transmits raw ASCII over UART (9600 / 115200 baud) |
| **Visual Indicators** | 5mm Diffused LEDs (1x Green, 1x Red) | `wokwi-led` (Green, Red) | Immediate visual feedback for access granted vs access denied |
| **Auditory Feedback** | Active Piezo Buzzer (5V / 3.3V) | `wokwi-buzzer` | Sound indicator: single short beep (success), double buzz (rejected / error) |
| **Actuator Control** | 5V 1-Channel Relay Module (Optocoupler) | `wokwi-relay-module` | Drives electronic turnstile or solenoid magnetic gate lock |
| **Input / Override** | Tactile Push Button | `wokwi-pushbutton` | Manual gate trigger or simulated hardware interrupt |
| **Power Supply** | 5V 2A DC Adapter (Micro-USB / 5V VIN) | Virtual 5V Rail | Regulated power delivery to ESP32 and peripheral modules |

## 2. NodeMCU RFID Variant (`nodemcu` build)
A lower-cost gate built from a NodeMCU V3 and an RC522 reader. Students tap a MIFARE Classic card that carries their HostelHive pass instead of showing a QR code. See `docs/pinout_mapping.md` for wiring.

| Component | Physical Model / Specification | Purpose / Interface |
|:---|:---|:---|
| **Microcontroller** | NodeMCU V3 (ESP-12E "ESP8266MOD", CH340 USB) | Wi-Fi, HTTPS (BearSSL), RFID and gate control |
| **RFID Reader** | RC522 (MFRC522) 13.56 MHz module, 3.3V | Reads MIFARE Classic 1K cards over SPI |
| **Student Cards** | MIFARE Classic 1K cards / key tags with the factory key | Carry the student's `HH1:` pass in sector 1 (written with the `provision` command) |
| **Auditory Feedback** | Active buzzer that works at 3.3 V (e.g. TMB12A12) | Beep patterns: granted, denied, duplicate, offline |
| **Actuator Control** (optional) | 5V 1-channel **active-LOW** relay module | Drives the solenoid lock |
| **Status / Override** | Onboard LED and onboard FLASH button | No extra parts needed |

The CH340 chip on many NodeMCU V3 clones only works with the older Windows driver (CH341SER 3.4.2014). Newer drivers (3.9, 4.0) enumerate the port but fail with "A device attached to the system is not functioning".
