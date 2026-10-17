#include <unity.h>
#include "../test_common.h"

#define TEST_COOLDOWN_MS 15000

struct TestCooldownEntry {
    String studentId;
    unsigned long timestampMillis;
};

TestCooldownEntry history[4];
int historyCount = 0;

void recordTestScan(const String& studentId, unsigned long now) {
    if (historyCount < 4) {
        history[historyCount].studentId = studentId;
        history[historyCount].timestampMillis = now;
        historyCount++;
    }
}

bool checkTestCooldown(const String& studentId, unsigned long now) {
    for (int i = 0; i < historyCount; i++) {
        if (history[i].studentId == studentId) {
            if ((now - history[i].timestampMillis) < TEST_COOLDOWN_MS) {
                return true; // Cooldown active
            }
        }
    }
    return false;
}

void test_initial_scan_allowed(void) {
    historyCount = 0;
    TEST_ASSERT_FALSE(checkTestCooldown("HH-STU-001", 1000));
}

void test_rapid_rescan_blocked(void) {
    historyCount = 0;
    recordTestScan("HH-STU-001", 1000);
    // Attempt scan after 3 seconds (less than 15s)
    TEST_ASSERT_TRUE(checkTestCooldown("HH-STU-001", 4000));
}

void test_scan_allowed_after_cooldown_expired(void) {
    historyCount = 0;
    recordTestScan("HH-STU-001", 1000);
    // Attempt scan after 16 seconds (exceeds 15s)
    TEST_ASSERT_FALSE(checkTestCooldown("HH-STU-001", 17000));
}

int runCooldownTests(void) {
    UNITY_BEGIN();
    RUN_TEST(test_initial_scan_allowed);
    RUN_TEST(test_rapid_rescan_blocked);
    RUN_TEST(test_scan_allowed_after_cooldown_expired);
    return UNITY_END();
}

#ifdef ARDUINO
void setup() {
    delay(2000);
    runCooldownTests();
}
void loop() {}
#else
int main(int argc, char **argv) {
    return runCooldownTests();
}
#endif
