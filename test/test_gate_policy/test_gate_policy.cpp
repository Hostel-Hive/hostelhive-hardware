#include <unity.h>
#include <string.h>
#include "gate_policy.h"

using gate::Decision;

// ---- Fail-closed decision table (H1 in SECURITY_AUDIT.md) ----

void test_unlock_only_on_200_with_known_outcome(void) {
    TEST_ASSERT_TRUE(gate::decide(200, "recorded") == Decision::UNLOCK);
    TEST_ASSERT_TRUE(gate::decide(200, "cooldown") == Decision::UNLOCK);
    TEST_ASSERT_TRUE(gate::decide(200, "already_in") == Decision::UNLOCK);
    TEST_ASSERT_TRUE(gate::decide(200, "already_out") == Decision::UNLOCK);
}

void test_200_without_valid_outcome_denies(void) {
    TEST_ASSERT_TRUE(gate::decide(200, NULL) == Decision::DENY_UNAVAILABLE);
    TEST_ASSERT_TRUE(gate::decide(200, "") == Decision::DENY_UNAVAILABLE);
    TEST_ASSERT_TRUE(gate::decide(200, "granted") == Decision::DENY_UNAVAILABLE);
}

void test_network_failures_deny(void) {
    // HTTPClient reports connection/timeout errors as negative codes.
    TEST_ASSERT_TRUE(gate::decide(0, NULL) == Decision::DENY_UNAVAILABLE);
    TEST_ASSERT_TRUE(gate::decide(-1, NULL) == Decision::DENY_UNAVAILABLE);
    TEST_ASSERT_TRUE(gate::decide(-11, NULL) == Decision::DENY_UNAVAILABLE);
}

void test_server_and_client_errors_deny(void) {
    TEST_ASSERT_TRUE(gate::decide(201, "recorded") == Decision::DENY_UNAVAILABLE);
    TEST_ASSERT_TRUE(gate::decide(302, NULL) == Decision::DENY_UNAVAILABLE);
    TEST_ASSERT_TRUE(gate::decide(500, NULL) == Decision::DENY_UNAVAILABLE);
    TEST_ASSERT_TRUE(gate::decide(503, NULL) == Decision::DENY_UNAVAILABLE);
    TEST_ASSERT_TRUE(gate::decide(422, NULL) == Decision::DENY_INVALID_PASS);
    TEST_ASSERT_TRUE(gate::decide(401, NULL) == Decision::DENY_DEVICE_UNAUTHORIZED);
    TEST_ASSERT_TRUE(gate::decide(403, NULL) == Decision::DENY_UNAVAILABLE);
    TEST_ASSERT_TRUE(gate::decide(400, NULL) == Decision::DENY_BAD_REQUEST);
}

void test_409_distinguishes_error_codes(void) {
    TEST_ASSERT_TRUE(gate::decide(409, NULL, "scanner_direction_required") == Decision::DENY_GATE_NOT_CONFIGURED);
    TEST_ASSERT_TRUE(gate::decide(409, NULL, "event_id_conflict") == Decision::DENY_BAD_REQUEST);
}

// ---- Device credential shape (M1) ----

static const char* GOOD_CREDENTIAL = "hhd1.3f2a8c4e-1b6d-4e9a-9c7f-2d5e8a1b4c6f.AbCdEfGhIjKlMnOpQrStUvWxYz0123456789-_abcdE";

void test_issued_credential_accepted(void) {
    TEST_ASSERT_EQUAL_INT(85, (int)strlen(GOOD_CREDENTIAL));
    TEST_ASSERT_TRUE(gate::isValidDeviceCredential(GOOD_CREDENTIAL));
}

void test_placeholders_and_legacy_keys_rejected(void) {
    TEST_ASSERT_FALSE(gate::isValidDeviceCredential(NULL));
    TEST_ASSERT_FALSE(gate::isValidDeviceCredential(""));
    TEST_ASSERT_FALSE(gate::isValidDeviceCredential("REPLACE_WITH_ISSUED_SCANNER_CREDENTIAL"));
    TEST_ASSERT_FALSE(gate::isValidDeviceCredential("hh_dev_sec_gate_entry_987654321"));
}

void test_malformed_credentials_rejected(void) {
    // uppercase UUID
    TEST_ASSERT_FALSE(gate::isValidDeviceCredential("hhd1.3F2A8C4E-1B6D-4E9A-9C7F-2D5E8A1B4C6F.AbCdEfGhIjKlMnOpQrStUvWxYz0123456789-_abcdE"));
    // nil UUID
    TEST_ASSERT_FALSE(gate::isValidDeviceCredential("hhd1.00000000-0000-0000-0000-000000000000.AbCdEfGhIjKlMnOpQrStUvWxYz0123456789-_abcdE"));
    // padding char in secret
    TEST_ASSERT_FALSE(gate::isValidDeviceCredential("hhd1.3f2a8c4e-1b6d-4e9a-9c7f-2d5e8a1b4c6f.AbCdEfGhIjKlMnOpQrStUvWxYz0123456789-_abcd="));
    // non-canonical final base64url char (padding bits set)
    TEST_ASSERT_FALSE(gate::isValidDeviceCredential("hhd1.3f2a8c4e-1b6d-4e9a-9c7f-2d5e8a1b4c6f.AbCdEfGhIjKlMnOpQrStUvWxYz0123456789-_abcdF"));
    // wrong version
    TEST_ASSERT_FALSE(gate::isValidDeviceCredential("hhd2.3f2a8c4e-1b6d-4e9a-9c7f-2d5e8a1b4c6f.AbCdEfGhIjKlMnOpQrStUvWxYz0123456789-_abcdE"));
}

// ---- Transport (M1) ----

void test_only_https_urls_accepted(void) {
    TEST_ASSERT_TRUE(gate::isHttpsUrl("https://api.hostelhive.example"));
    TEST_ASSERT_FALSE(gate::isHttpsUrl("http://192.168.1.100:8080"));
    TEST_ASSERT_FALSE(gate::isHttpsUrl("https://"));
    TEST_ASSERT_FALSE(gate::isHttpsUrl("HTTPS://api.example.com"));
    TEST_ASSERT_FALSE(gate::isHttpsUrl(NULL));
}

// ---- Event IDs accepted by the backend's canonical UUID check ----

void test_uuid_v4_format(void) {
    uint8_t zeros[16];
    memset(zeros, 0, sizeof(zeros));
    char out[37];
    gate::formatUuidV4(zeros, out);
    TEST_ASSERT_EQUAL_STRING("00000000-0000-4000-8000-000000000000", out);
    TEST_ASSERT_TRUE(gate::isCanonicalUuid(out, strlen(out)));

    uint8_t ones[16];
    memset(ones, 0xFF, sizeof(ones));
    gate::formatUuidV4(ones, out);
    TEST_ASSERT_EQUAL_STRING("ffffffff-ffff-4fff-bfff-ffffffffffff", out);
    TEST_ASSERT_TRUE(gate::isCanonicalUuid(out, strlen(out)));
}

int runUnityTests(void) {
    UNITY_BEGIN();
    RUN_TEST(test_unlock_only_on_200_with_known_outcome);
    RUN_TEST(test_200_without_valid_outcome_denies);
    RUN_TEST(test_network_failures_deny);
    RUN_TEST(test_server_and_client_errors_deny);
    RUN_TEST(test_409_distinguishes_error_codes);
    RUN_TEST(test_issued_credential_accepted);
    RUN_TEST(test_placeholders_and_legacy_keys_rejected);
    RUN_TEST(test_malformed_credentials_rejected);
    RUN_TEST(test_only_https_urls_accepted);
    RUN_TEST(test_uuid_v4_format);
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
