#include <unity.h>
#include <string.h>
#include "gate_policy.h"

// Exercises the shipped validator in include/gate_policy.h (not a mock).

static bool valid(const char* s) { return gate::isValidQrPayload(s, strlen(s)); }

static const char* GOOD = "HH1:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

void test_backend_issued_payload_accepted(void) {
    TEST_ASSERT_TRUE(valid(GOOD));
}

void test_empty_payload_rejected(void) {
    TEST_ASSERT_FALSE(valid(""));
    TEST_ASSERT_FALSE(gate::isValidQrPayload(NULL, 0));
}

void test_legacy_student_ids_rejected(void) {
    TEST_ASSERT_FALSE(valid("HH-STU-2021-CS-045"));
    TEST_ASSERT_FALSE(valid("HH:STU-001"));
    TEST_ASSERT_FALSE(valid("STU12345"));
    TEST_ASSERT_FALSE(valid("any random barcode"));
}

void test_wrong_prefix_rejected(void) {
    TEST_ASSERT_FALSE(valid("HH2:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"));
    TEST_ASSERT_FALSE(valid("hh1:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"));
}

void test_uppercase_or_non_hex_rejected(void) {
    TEST_ASSERT_FALSE(valid("HH1:0123456789ABCDEF0123456789abcdef0123456789abcdef0123456789abcdef"));
    TEST_ASSERT_FALSE(valid("HH1:0123456789abcdeg0123456789abcdef0123456789abcdef0123456789abcdef"));
}

void test_wrong_length_rejected(void) {
    TEST_ASSERT_FALSE(valid("HH1:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde"));
    TEST_ASSERT_FALSE(valid("HH1:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0"));
}

void test_embedded_whitespace_rejected(void) {
    TEST_ASSERT_FALSE(valid("HH1:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde "));
    TEST_ASSERT_FALSE(valid(" HH1:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcd"));
}

int runUnityTests(void) {
    UNITY_BEGIN();
    RUN_TEST(test_backend_issued_payload_accepted);
    RUN_TEST(test_empty_payload_rejected);
    RUN_TEST(test_legacy_student_ids_rejected);
    RUN_TEST(test_wrong_prefix_rejected);
    RUN_TEST(test_uppercase_or_non_hex_rejected);
    RUN_TEST(test_wrong_length_rejected);
    RUN_TEST(test_embedded_whitespace_rejected);
    return UNITY_END();
}

#ifdef ARDUINO
#include <Arduino.h>
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
