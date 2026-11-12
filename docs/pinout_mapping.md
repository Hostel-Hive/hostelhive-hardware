# Pinout & GPIO Mapping Specification
**Project:** HostelHive IoT Attendance Node

Two builds share the same firmware: the ESP32 QR gate (`esp32dev` / `esp32prod`) and the NodeMCU RFID gate (`nodemcu` / `nodemcu_dev`). Pins are defined in `include/config.h`.

## ESP32-WROOM-32 DevKit V1 (QR scanner)

| Peripheral Module | ESP32 GPIO Pin | Pin Mode | Logic Level | Description |
|:---|:---|:---|:---|:---|
| **OLED Display (SDA)** | `GPIO 21` | I2C SDA | 3.3V | I2C Data bus for SSD1306 128x64 display |
| **OLED Display (SCL)** | `GPIO 22` | I2C SCL | 3.3V | I2C Clock bus for SSD1306 128x64 display |
| **Status LED: Green** | `GPIO 18` | `OUTPUT` | Active HIGH | Illuminates on valid scan & access granted |
| **Status LED: Red** | `GPIO 19` | `OUTPUT` | Active HIGH | Illuminates on invalid scan, cooldown or error |
| **Piezo Buzzer** | `GPIO 23` | `OUTPUT` (PWM) | Active HIGH | Emits success chirp (2400 Hz) or denial alert (450 Hz) |
| **Gate Lock Relay** | `GPIO 27` | `OUTPUT` | Active HIGH | Triggers solenoid strike or turnstile pulse (3s duration) |
| **QR Scanner (RX)** | `GPIO 16` (RX2) | `INPUT` | 3.3V UART | Receives scanned barcode/QR payload from scanner module |
| **QR Scanner (TX)** | `GPIO 17` (TX2) | `OUTPUT` | 3.3V UART | Configuration commands sent to scanner module |
| **Manual Trigger Button**| `GPIO 34` | `INPUT` + external 10k pull-up | Active LOW | Emergency gate release or manual override button |

### Power Distribution
* **VIN / 5V**: Powers 5V Relay VCC, Buzzer, and scanner module logic.
* **3V3**: Powers SSD1306 OLED display logic.
* **GND**: Common ground plane across ESP32, OLED, Relay, and Scanner.

## NodeMCU V3 (ESP8266 / ESP-12E, CH340) (RC522 RFID reader)

| Peripheral Module | NodeMCU Pin | GPIO | Notes |
|:---|:---|:---|:---|
| **RC522 SDA (SS)** | `D8` | 15 | SPI chip select |
| **RC522 SCK** | `D5` | 14 | Hardware SPI clock |
| **RC522 MOSI** | `D7` | 13 | Hardware SPI |
| **RC522 MISO** | `D6` | 12 | Hardware SPI |
| **RC522 RST** | `D1` | 5 | Reader reset |
| **RC522 IRQ** | not connected | | Polled, not interrupt driven |
| **RC522 3.3V / GND** | `3V` / `G` | | **3.3V only.** 5V damages the RC522 |
| **Buzzer (+)** | `D2` | 4 | Active buzzer (e.g. TMB12A12) directly; passive piezo through ~100 Ω. `-` to `G`. LOW at boot, so silent on reset |
| **Gate Lock Relay IN** | `D0` | 16 | Use an **active-LOW** relay module: GPIO16 is HIGH during boot, which keeps an active-LOW relay off. Relay VCC from `VIN` (5V) |
| **Status LED** | onboard (`D4`) | 2 | Blue LED next to the antenna, lit on LOW |
| **Manual Override** | onboard `FLASH` button (`D3`) | 0 | Hold 1 s to unlock (logged). Don't hold it while powering on: the board would enter flash mode |

No OLED or external LEDs: gate status is printed to the USB serial log as `[GATE] ...` lines.

A magnetic or "active" buzzer draws more than an ESP8266 pin can supply (~12 mA). Drive it through an NPN transistor (BC547 / 2N2222, 1 kΩ base resistor) instead of connecting it directly.
