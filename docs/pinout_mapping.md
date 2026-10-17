# ESP32 Pinout & GPIO Mapping Specification
**Project:** HostelHive IoT Attendance Node  
**Hardware Platform:** ESP32-WROOM-32 DevKit V1

## Pinout Assignment Table

| Peripheral Module | ESP32 GPIO Pin | Pin Mode | Logic Level | Description |
|:---|:---|:---|:---|:---|
| **OLED Display (SDA)** | `GPIO 21` | I2C SDA | 3.3V | I2C Data bus for SSD1306 128x64 display |
| **OLED Display (SCL)** | `GPIO 22` | I2C SCL | 3.3V | I2C Clock bus for SSD1306 128x64 display |
| **Status LED: Green** | `GPIO 18` | `OUTPUT` | Active HIGH | Illuminates on valid scan & access granted |
| **Status LED: Red** | `GPIO 19` | `OUTPUT` | Active HIGH | Illuminates on invalid scan, cooldown or error |
| **Piezo Buzzer** | `GPIO 23` | `OUTPUT` (PWM) | Active HIGH | Emits success chirp (2000 Hz) or denial alert (800 Hz) |
| **Gate Lock Relay** | `GPIO 27` | `OUTPUT` | Active HIGH / LOW (Inverted) | Triggers solenoid strike or turnstile pulse (3s duration) |
| **QR Scanner (RX)** | `GPIO 16` (RX2) | `INPUT` | 3.3V UART | Receives scanned barcode/QR payload from scanner module |
| **QR Scanner (TX)** | `GPIO 17` (TX2) | `OUTPUT` | 3.3V UART | Configuration commands sent to scanner module |
| **Manual Trigger Button**| `GPIO 34` | `INPUT_PULLUP`| Active LOW | Emergency gate release or manual override button |

## Power Distribution
* **VIN / 5V**: Powers 5V Relay VCC, Buzzer, and scanner module logic.
* **3V3**: Powers SSD1306 OLED display logic.
* **GND**: Common ground plane across ESP32, OLED, Relay, and Scanner.
