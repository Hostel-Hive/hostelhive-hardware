#pragma once
#ifndef HOSTELHIVE_CARD_FORMAT_H
#define HOSTELHIVE_CARD_FORMAT_H

// How a student's HostelHive pass is stored on a MIFARE Classic card, kept
// free of Arduino dependencies so the native unit tests exercise exactly the
// code that ships on the device.
//
// The card carries the same "HH1:<64 hex>" pass the backend issues as a QR
// code, so the gate sends it to POST /api/v1/device/scans unchanged and the
// backend needs no RFID support. The card UID is never used to identify a
// student, because UIDs are trivially cloned.
//
// Layout in sector 1 (default key A FF FF FF FF FF FF), 48 bytes:
//   block 4  header: 'H' 'H' 'C' '1', format version, CRC-8 of the token, 10 zero bytes
//   block 5  token bytes 0..15
//   block 6  token bytes 16..31
// Block 7 is the sector trailer (keys + access bits) and is never written:
// a bad write there can lock the sector permanently.
//
// Like a printed QR code, the pass on a card can be copied by anyone who reads
// it. The backend still checks the student is active on every scan.

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "gate_policy.h"

namespace cardfmt {

static const uint8_t BLOCK_HEADER = 4;
static const uint8_t BLOCK_TOKEN_A = 5;
static const uint8_t BLOCK_TOKEN_B = 6;
static const size_t BLOCK_SIZE = 16;
static const size_t DATA_SIZE = 3 * BLOCK_SIZE;  // 48
static const size_t TOKEN_BYTES = gate::QR_TOKEN_HEX_LEN / 2;  // 32

static const uint8_t MAGIC[4] = {'H', 'H', 'C', '1'};
static const uint8_t FORMAT_VERSION = 1;
static const size_t HEADER_VERSION_INDEX = 4;
static const size_t HEADER_CRC_INDEX = 5;

// MIFARE Classic 1K/Mini: every 4th block (3, 7, 11, ...) is a sector trailer.
// 4K cards switch to 16-block sectors from block 128.
inline bool isSectorTrailer(uint8_t block) {
    return block < 128 ? (block % 4) == 3 : (block % 16) == 15;
}

// CRC-8 (polynomial 0x07, init 0x00) over the token, to catch half-written cards.
inline uint8_t crc8(const uint8_t* data, size_t len) {
    uint8_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

inline int hexNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

// Builds the 48 bytes for blocks 4..6. Returns false (and writes nothing
// useful) unless `payload` is a well-formed backend-issued pass.
inline bool encode(const char* payload, size_t len, uint8_t out[DATA_SIZE]) {
    if (!gate::isValidQrPayload(payload, len)) return false;
    memset(out, 0, DATA_SIZE);
    uint8_t* token = out + BLOCK_SIZE;
    const char* hex = payload + gate::QR_PREFIX_LEN;
    for (size_t i = 0; i < TOKEN_BYTES; i++) {
        token[i] = (uint8_t)((hexNibble(hex[2 * i]) << 4) | hexNibble(hex[2 * i + 1]));
    }
    memcpy(out, MAGIC, sizeof(MAGIC));
    out[HEADER_VERSION_INDEX] = FORMAT_VERSION;
    out[HEADER_CRC_INDEX] = crc8(token, TOKEN_BYTES);
    return true;
}

enum class DecodeResult {
    OK,
    NOT_HOSTELHIVE,  // no HostelHive header: blank or someone else's card
    CORRUPT          // HostelHive header, but wrong version, CRC or reserved bytes
};

// Reads the 48 bytes of blocks 4..6 back into "HH1:<64 lowercase hex>".
// `out` must hold gate::QR_PAYLOAD_LEN + 1 chars and is only valid on OK.
inline DecodeResult decode(const uint8_t in[DATA_SIZE], char out[gate::QR_PAYLOAD_LEN + 1]) {
    if (memcmp(in, MAGIC, sizeof(MAGIC)) != 0) return DecodeResult::NOT_HOSTELHIVE;
    if (in[HEADER_VERSION_INDEX] != FORMAT_VERSION) return DecodeResult::CORRUPT;
    for (size_t i = HEADER_CRC_INDEX + 1; i < BLOCK_SIZE; i++) {
        if (in[i] != 0) return DecodeResult::CORRUPT;
    }
    const uint8_t* token = in + BLOCK_SIZE;
    if (in[HEADER_CRC_INDEX] != crc8(token, TOKEN_BYTES)) return DecodeResult::CORRUPT;

    static const char hex[] = "0123456789abcdef";
    memcpy(out, gate::QR_PREFIX, gate::QR_PREFIX_LEN);
    for (size_t i = 0; i < TOKEN_BYTES; i++) {
        out[gate::QR_PREFIX_LEN + 2 * i] = hex[token[i] >> 4];
        out[gate::QR_PREFIX_LEN + 2 * i + 1] = hex[token[i] & 0x0F];
    }
    out[gate::QR_PAYLOAD_LEN] = '\0';
    return gate::isValidQrPayload(out, gate::QR_PAYLOAD_LEN) ? DecodeResult::OK : DecodeResult::CORRUPT;
}

}  // namespace cardfmt

#endif  // HOSTELHIVE_CARD_FORMAT_H
