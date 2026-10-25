#include <unity.h>
#include "../test_common.h"

// Mock / Standalone test functions for QR parsing
struct MockScanResult {
    bool isValid;
    String studentId;
    String errorReason;
};

MockScanResult parseTestQr(const String& rawData) {
    MockScanResult result;
    result.isValid = false;
    result.studentId = "";
    result.errorReason = "";

    String sanitized = rawData;
    sanitized.replace("\r", "");
    sanitized.replace("\n", "");
    sanitized.trim();

    if (sanitized.length() == 0) {
        result.errorReason = "Empty QR payload";
        return result;
    }

    if (sanitized.length() < 4) {
        result.errorReason = "Payload too short";
        return result;
    }

    result.studentId = sanitized;
    result.isValid = true;
    return result;
}

void test_valid_student_id_prefix(void) {
    MockScanResult res = parseTestQr("HH-STU-2021-CS-045\r\n");
    TEST_ASSERT_TRUE(res.isValid);
    TEST_ASSERT_EQUAL_STRING("HH-STU-2021-CS-045", res.studentId.c_str());
}

void test_empty_payload_rejection(void) {
    MockScanResult res = parseTestQr("   \r\n");
    TEST_ASSERT_FALSE(res.isValid);
    TEST_ASSERT_EQUAL_STRING("Empty QR payload", res.errorReason.c_str());
}

void test_too_short_payload(void) {
    MockScanResult res = parseTestQr("AB\n");
    TEST_ASSERT_FALSE(res.isValid);
    TEST_ASSERT_EQUAL_STRING("Payload too short", res.errorReason.c_str());
}

int runUnityTests(void) {
    UNITY_BEGIN();
    RUN_TEST(test_valid_student_id_prefix);
    RUN_TEST(test_empty_payload_rejection);
    RUN_TEST(test_too_short_payload);
    return UNITY_END();
}

#ifdef ARDUINO
void setup() {
    delay(2000);
    runUnityTests();
}
void loop() {}
#else
int main(int argc, char **argv) {
    return runUnityTests();
}
#endif
