#include <Arduino.h>
#include "config.h"
#include "buzzer_led.h"
#include "relay_control.h"

// Hardware smoke test interval
unsigned long lastTestCycle = 0;
int testStep = 0;

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n==========================================");
    Serial.println(" HostelHive IoT Node - Hardware Test Stage");
    Serial.println(" Node: " NODE_ID " | Direction: " GATE_DIRECTION);
    Serial.println("==========================================");

    feedbackController.init();
    relayController.init();

    Serial.println("[SETUP] Feedback and Relay actuators initialized.");
}

void loop() {
    feedbackController.update();
    relayController.update();

    // Run periodic actuator diagnostic cycle every 6 seconds
    if (millis() - lastTestCycle >= 6000) {
        lastTestCycle = millis();

        if (testStep == 0) {
            Serial.println("[DIAGNOSTIC] Testing Access Granted sequence: Green LED + Success Chirp + Relay Unlock");
            feedbackController.triggerFeedback(FeedbackType::SUCCESS_GRANTED);
            relayController.unlockGate(2000);
            testStep = 1;
        } else {
            Serial.println("[DIAGNOSTIC] Testing Access Denied sequence: Red LED + Denial Alert Tone");
            feedbackController.triggerFeedback(FeedbackType::ACCESS_DENIED);
            testStep = 0;
        }
    }
}
