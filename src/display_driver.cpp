#include "display_driver.h"
#include "config.h"

DisplayDriver displayDriver;

DisplayDriver::DisplayDriver()
    : display(OLED_SCREEN_WIDTH, OLED_SCREEN_HEIGHT, &Wire, OLED_RESET_PIN),
      isAvailable(false) {}

bool DisplayDriver::init() {
    Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);

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

void DisplayDriver::showIdleScreen(const String& ipAddress, bool wifiConnected, int queueCount) {
    if (!isAvailable) return;

    display.clearDisplay();
    // Header
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print(NODE_ID);
    display.setCursor(95, 0);
    display.print(wifiConnected ? "[ON]" : "[OFF]");

    display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

    // Center Prompt
    display.setTextSize(2);
    display.setCursor(20, 20);
    display.println("SCAN ID");

    display.setTextSize(1);
    display.setCursor(12, 38);
    display.println("Place QR in front");

    // Footer
    display.drawFastHLine(0, 50, 128, SSD1306_WHITE);
    display.setCursor(0, 54);
    if (queueCount > 0) {
        display.print("Queue: ");
        display.print(queueCount);
    } else {
        display.print(wifiConnected ? "Ready" : "Offline Buffer");
    }

    display.setCursor(85, 54);
    display.print("Mode:");
    display.print(GATE_DIRECTION);

    display.display();
}

void DisplayDriver::showScanningScreen() {
    if (!isAvailable) return;

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(25, 10);
    display.println("READING QR CODE");

    display.drawRect(20, 26, 88, 16, SSD1306_WHITE);
    display.fillRect(24, 30, 40, 8, SSD1306_WHITE);

    display.setCursor(32, 48);
    display.println("Please hold...");
    display.display();
}

void DisplayDriver::showVerifyingScreen(const String& studentId) {
    if (!isAvailable) return;

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(16, 5);
    display.println("AUTHENTICATING");
    display.drawFastHLine(0, 16, 128, SSD1306_WHITE);

    display.setCursor(5, 24);
    display.print("ID: ");
    display.println(studentId);

    display.setCursor(5, 40);
    display.println("Checking Backend...");
    display.display();
}

void DisplayDriver::showAccessGranted(const String& studentName, const String& studentId, const String& room) {
    if (!isAvailable) return;

    display.clearDisplay();
    display.fillRect(0, 0, 128, 14, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(18, 3);
    display.println("ACCESS GRANTED");

    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 20);
    display.println(studentName);

    display.setCursor(0, 34);
    display.print("ID: ");
    display.println(studentId);

    display.setCursor(0, 48);
    display.print("Room: ");
    display.print(room);
    display.print(" | ");
    display.println(GATE_DIRECTION);

    display.display();
}

void DisplayDriver::showAccessDenied(const String& reason) {
    if (!isAvailable) return;

    display.clearDisplay();
    display.fillRect(0, 0, 128, 14, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(22, 3);
    display.println("ACCESS DENIED");

    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 24);
    display.println(reason);

    display.setCursor(0, 44);
    display.println("Contact Warden Office");
    display.display();
}

void DisplayDriver::showCooldownScreen(unsigned long remainingSeconds) {
    if (!isAvailable) return;

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(20, 10);
    display.println("DUPLICATE SCAN");
    display.drawFastHLine(0, 22, 128, SSD1306_WHITE);

    display.setCursor(15, 30);
    display.print("Cooldown active: ");
    display.print(remainingSeconds);
    display.println("s");

    display.setCursor(15, 45);
    display.println("Please pass through.");
    display.display();
}

void DisplayDriver::showOfflineScreen(int queuedCount) {
    if (!isAvailable) return;

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(10, 5);
    display.println("OFFLINE RECORDING");
    display.drawFastHLine(0, 16, 128, SSD1306_WHITE);

    display.setCursor(0, 26);
    display.println("Scan saved locally.");

    display.setCursor(0, 42);
    display.print("Pending Queue: ");
    display.println(queuedCount);

    display.display();
}

void DisplayDriver::clear() {
    if (isAvailable) {
        display.clearDisplay();
        display.display();
    }
}
