#include "display_driver.h"
#include "config.h"

DisplayDriver displayDriver;

DisplayDriver::DisplayDriver()
    : display(OLED_SCREEN_WIDTH, OLED_SCREEN_HEIGHT, &Wire, OLED_RESET_PIN),
      isAvailable(false) {}

bool DisplayDriver::init() {
    Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);

    // Initialize SSD1306 on 0x3C (common default for 128x64)
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("[ERROR] SSD1306 OLED initialization failed!");
        isAvailable = false;
        return false;
    }

    isAvailable = true;
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    showBootScreen();
    return true;
}

void DisplayDriver::showBootScreen() {
    if (!isAvailable) return;

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(18, 10);
    display.println("HOSTEL HIVE");

    display.drawFastHLine(10, 22, 108, SSD1306_WHITE);

    display.setCursor(12, 28);
    display.print("Node: ");
    display.println(NODE_ID);

    display.setCursor(12, 40);
    display.print("Firmware: v");
    display.println(HOSTELHIVE_FIRMWARE_VERSION);

    display.setCursor(12, 52);
    display.println("Booting Subsystems...");
    display.display();
}

void DisplayDriver::clear() {
    if (isAvailable) {
        display.clearDisplay();
        display.display();
    }
}
