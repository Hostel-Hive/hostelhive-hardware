#include <unity.h>
#include <string.h>
#include "card_format.h"

// Exercises the shipped card layout in include/card_format.h (not a mock).

static const char* PASS = "HH1:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

static void encodeGood(uint8_t data[cardfmt::DATA_SIZE]) {
    TEST_ASSERT_TRUE(cardfmt::encode(PASS, strlen(PASS), data));
}

void test_round_trip_returns_same_pass(void) {
    uint8_t data[cardfmt::DATA_SIZE];
    encodeGood(data);
    char out[gate::QR_PAYLOAD_LEN + 1];
    TEST_ASSERT_TRUE(cardfmt::decode(data, out) == cardfmt::DecodeResult::OK);
    TEST_ASSERT_EQUAL_STRING(PASS, out);
}

void test_header_layout(void) {
    uint8_t data[cardfmt::DATA_SIZE];
    encodeGood(data);
    TEST_ASSERT_EQUAL_MEMORY("HHC1", data, 4);
    TEST_ASSERT_EQUAL_UINT8(cardfmt::FORMAT_VERSION, data[4]);
    for (size_t i = 6; i < cardfmt::BLOCK_SIZE; i++) TEST_ASSERT_EQUAL_UINT8(0, data[i]);
    // Token stored as raw bytes: 0x01 0x23 0x45 ...
    TEST_ASSERT_EQUAL_UINT8(0x01, data[16]);
    TEST_ASSERT_EQUAL_UINT8(0x23, data[17]);
    TEST_ASSERT_EQUAL_UINT8(0xEF, data[47]);
}

void test_encode_rejects_non_passes(void) {
    uint8_t data[cardfmt::DATA_SIZE];
    const char* bad[] = {"", "HH-STU-2021-CS-045",
                         "HH1:0123456789ABCDEF0123456789abcdef0123456789abcdef0123456789abcdef",
                         "HH1:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde"};
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        TEST_ASSERT_FALSE(cardfmt::encode(bad[i], strlen(bad[i]), data));
    }
}

void test_blank_and_foreign_cards_are_not_hostelhive(void) {
    uint8_t data[cardfmt::DATA_SIZE];
    char out[gate::QR_PAYLOAD_LEN + 1];
    memset(data, 0x00, sizeof(data));
    TEST_ASSERT_TRUE(cardfmt::decode(data, out) == cardfmt::DecodeResult::NOT_HOSTELHIVE);
    memset(data, 0xFF, sizeof(data));
    TEST_ASSERT_TRUE(cardfmt::decode(data, out) == cardfmt::DecodeResult::NOT_HOSTELHIVE);
}

void test_flipped_token_bit_is_corrupt(void) {
    uint8_t data[cardfmt::DATA_SIZE];
    char out[gate::QR_PAYLOAD_LEN + 1];
    encodeGood(data);
    data[30] ^= 0x01;
    TEST_ASSERT_TRUE(cardfmt::decode(data, out) == cardfmt::DecodeResult::CORRUPT);
}

void test_half_written_card_is_corrupt(void) {
    // Header from one pass, token block B still zero from a blank card.
    uint8_t data[cardfmt::DATA_SIZE];
    char out[gate::QR_PAYLOAD_LEN + 1];
    encodeGood(data);
    memset(data + 2 * cardfmt::BLOCK_SIZE, 0, cardfmt::BLOCK_SIZE);
    TEST_ASSERT_TRUE(cardfmt::decode(data, out) == cardfmt::DecodeResult::CORRUPT);
}

void test_unknown_version_or_reserved_bytes_are_corrupt(void) {
    uint8_t data[cardfmt::DATA_SIZE];
    char out[gate::QR_PAYLOAD_LEN + 1];
    encodeGood(data);
    data[4] = 2;
    TEST_ASSERT_TRUE(cardfmt::decode(data, out) == cardfmt::DecodeResult::CORRUPT);
    encodeGood(data);
    data[15] = 0x80;
    TEST_ASSERT_TRUE(cardfmt::decode(data, out) == cardfmt::DecodeResult::CORRUPT);
}

void test_data_blocks_are_never_sector_trailers(void) {
    TEST_ASSERT_FALSE(cardfmt::isSectorTrailer(cardfmt::BLOCK_HEADER));
    TEST_ASSERT_FALSE(cardfmt::isSectorTrailer(cardfmt::BLOCK_TOKEN_A));
    TEST_ASSERT_FALSE(cardfmt::isSectorTrailer(cardfmt::BLOCK_TOKEN_B));
    TEST_ASSERT_TRUE(cardfmt::isSectorTrailer(3));
    TEST_ASSERT_TRUE(cardfmt::isSectorTrailer(7));
    TEST_ASSERT_TRUE(cardfmt::isSectorTrailer(63));
    TEST_ASSERT_TRUE(cardfmt::isSectorTrailer(143));  // 4K large sector
    TEST_ASSERT_FALSE(cardfmt::isSectorTrailer(131));
}

void test_crc8_known_value(void) {
    // CRC-8/SMBUS (poly 0x07) check value for "123456789".
    TEST_ASSERT_EQUAL_UINT8(0xF4, cardfmt::crc8((const uint8_t*)"123456789", 9));
}

int runUnityTests(void) {
    UNITY_BEGIN();
    RUN_TEST(test_round_trip_returns_same_pass);
    RUN_TEST(test_header_layout);
    RUN_TEST(test_encode_rejects_non_passes);
    RUN_TEST(test_blank_and_foreign_cards_are_not_hostelhive);
    RUN_TEST(test_flipped_token_bit_is_corrupt);
    RUN_TEST(test_half_written_card_is_corrupt);
    RUN_TEST(test_unknown_version_or_reserved_bytes_are_corrupt);
    RUN_TEST(test_data_blocks_are_never_sector_trailers);
    RUN_TEST(test_crc8_known_value);
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
int main(int argc, char** argv) {
    return runUnityTests();
}
#endif
