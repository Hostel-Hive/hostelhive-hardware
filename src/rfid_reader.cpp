#include "rfid_reader.h"

#if HAS_RFID

#include <SPI.h>
#include <MFRC522.h>
#include "card_format.h"
#include "gate_policy.h"

RfidReader rfidReader;

static MFRC522 mfrc(PIN_RFID_SS, PIN_RFID_RST);

static const uint8_t DATA_BLOCKS[3] = {cardfmt::BLOCK_HEADER, cardfmt::BLOCK_TOKEN_A, cardfmt::BLOCK_TOKEN_B};

static MFRC522::MIFARE_Key defaultKey() {
    MFRC522::MIFARE_Key key;
    for (uint8_t i = 0; i < MFRC522::MF_KEY_SIZE; i++) key.keyByte[i] = 0xFF;
    return key;
}

static String uidHex(const MFRC522::Uid& uid) {
    static const char hex[] = "0123456789ABCDEF";
    String s;
    for (uint8_t i = 0; i < uid.size; i++) {
        s += hex[uid.uidByte[i] >> 4];
        s += hex[uid.uidByte[i] & 0x0F];
    }
    return s;
}

// A card that walks away mid-transaction shows up as a timeout or collision.
static bool isTransient(MFRC522::StatusCode status) {
    return status == MFRC522::STATUS_TIMEOUT || status == MFRC522::STATUS_COLLISION;
}

static CardRead result(CardStatus status, const String& uid) {
    CardRead r;
    r.status = status;
    r.payload = "";
    r.uid = uid;
    return r;
}

RfidReader::RfidReader()
    : mode(Mode::READ), pendingPass(""), armedAt(0), lastHealthCheck(0), readerOk(false), version(0) {}

bool RfidReader::init() {
    SPI.begin();
    mfrc.PCD_Init();
    delay(50);
    lastHealthCheck = millis();
    return checkReader(true);
}

bool RfidReader::checkReader(bool log) {
    version = mfrc.PCD_ReadRegister(MFRC522::VersionReg);
    readerOk = version != 0x00 && version != 0xFF;
    if (log) {
        if (readerOk) {
            Serial.printf("[RFID] RC522 ready (firmware 0x%02X) on SS=D8, RST=D1.\n", version);
        } else {
            Serial.printf("[RFID] RC522 not responding (0x%02X). Check SDA/SCK/MOSI/MISO/RST wiring and 3.3V.\n",
                          version);
        }
    }
    return readerOk;
}

bool RfidReader::armProvision(const String& pass) {
    if (!gate::isValidQrPayload(pass.c_str(), pass.length())) return false;
    pendingPass = pass;
    mode = Mode::PROVISION;
    armedAt = millis();
    return true;
}

void RfidReader::armWipe() {
    pendingPass = "";
    mode = Mode::WIPE;
    armedAt = millis();
}

void RfidReader::cancel() {
    pendingPass = "";
    mode = Mode::READ;
}

CardRead RfidReader::poll() {
    unsigned long now = millis();

    if (isArmed() && now - armedAt >= RFID_PROVISION_TIMEOUT_MS) {
        cancel();
        return result(CardStatus::PROVISION_TIMEOUT, "");
    }

    // The RC522 occasionally stops answering after brown-outs or ESD; re-init it.
    if (now - lastHealthCheck >= RFID_HEALTH_CHECK_MS) {
        lastHealthCheck = now;
        bool wasOk = readerOk;
        if (!checkReader(false)) {
            mfrc.PCD_Init();
            checkReader(wasOk);
        } else if (!wasOk) {
            Serial.println("[RFID] RC522 is responding again.");
        }
    }

    if (!readerOk) return result(CardStatus::NONE, "");
    // A card that stays on the reader is halted below and is not re-read until it is lifted.
    if (!mfrc.PICC_IsNewCardPresent() || !mfrc.PICC_ReadCardSerial()) return result(CardStatus::NONE, "");

    String uid = uidHex(mfrc.uid);
    MFRC522::PICC_Type type = mfrc.PICC_GetType(mfrc.uid.sak);
    CardRead r;
    if (type != MFRC522::PICC_TYPE_MIFARE_MINI && type != MFRC522::PICC_TYPE_MIFARE_1K &&
        type != MFRC522::PICC_TYPE_MIFARE_4K) {
        Serial.printf("[RFID] Card %s is not MIFARE Classic (%s).\n", uid.c_str(),
                      String(MFRC522::PICC_GetTypeName(type)).c_str());
        r = result(isArmed() ? CardStatus::PROVISION_FAILED : CardStatus::UNSUPPORTED, uid);
    } else if (isArmed()) {
        r = writeCard(uid);
    } else {
        r = readCard(uid);
    }

    mfrc.PICC_HaltA();
    mfrc.PCD_StopCrypto1();
    return r;
}

CardRead RfidReader::readCard(const String& uid) {
    MFRC522::MIFARE_Key key = defaultKey();
    MFRC522::StatusCode status =
        mfrc.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, cardfmt::BLOCK_HEADER, &key, &mfrc.uid);
    if (status != MFRC522::STATUS_OK) {
        Serial.printf("[RFID] Card %s: sector 1 auth failed (%s).\n", uid.c_str(),
                      String(MFRC522::GetStatusCodeName(status)).c_str());
        return result(isTransient(status) ? CardStatus::READ_ERROR : CardStatus::NOT_REGISTERED, uid);
    }

    uint8_t data[cardfmt::DATA_SIZE];
    for (uint8_t i = 0; i < 3; i++) {
        uint8_t buffer[18];
        uint8_t size = sizeof(buffer);
        status = mfrc.MIFARE_Read(DATA_BLOCKS[i], buffer, &size);
        if (status != MFRC522::STATUS_OK) {
            Serial.printf("[RFID] Card %s: read of block %u failed (%s).\n", uid.c_str(), DATA_BLOCKS[i],
                          String(MFRC522::GetStatusCodeName(status)).c_str());
            return result(CardStatus::READ_ERROR, uid);
        }
        memcpy(data + i * cardfmt::BLOCK_SIZE, buffer, cardfmt::BLOCK_SIZE);
    }

    char pass[gate::QR_PAYLOAD_LEN + 1];
    switch (cardfmt::decode(data, pass)) {
        case cardfmt::DecodeResult::OK: {
            CardRead r = result(CardStatus::PASS, uid);
            r.payload = String(pass);
            return r;
        }
        case cardfmt::DecodeResult::CORRUPT:
            Serial.printf("[RFID] Card %s has damaged HostelHive data. Provision it again.\n", uid.c_str());
            return result(CardStatus::CORRUPT, uid);
        default:
            Serial.printf("[RFID] Card %s has no HostelHive pass.\n", uid.c_str());
            return result(CardStatus::NOT_REGISTERED, uid);
    }
}

CardRead RfidReader::writeCard(const String& uid) {
    uint8_t data[cardfmt::DATA_SIZE];
    if (mode == Mode::WIPE) {
        memset(data, 0, sizeof(data));
    } else if (!cardfmt::encode(pendingPass.c_str(), pendingPass.length(), data)) {
        cancel();
        return result(CardStatus::PROVISION_FAILED, uid);
    }

    MFRC522::MIFARE_Key key = defaultKey();
    MFRC522::StatusCode status =
        mfrc.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, cardfmt::BLOCK_HEADER, &key, &mfrc.uid);
    if (status != MFRC522::STATUS_OK) {
        Serial.printf("[RFID] Card %s: sector 1 auth failed (%s). Only cards with the factory key can be written.\n",
                      uid.c_str(), String(MFRC522::GetStatusCodeName(status)).c_str());
        return result(CardStatus::PROVISION_FAILED, uid);
    }

    // Token blocks first, header last: an interrupted write leaves a CRC
    // mismatch (CORRUPT) instead of a valid-looking pass.
    static const uint8_t WRITE_ORDER[3] = {cardfmt::BLOCK_TOKEN_A, cardfmt::BLOCK_TOKEN_B, cardfmt::BLOCK_HEADER};
    for (uint8_t i = 0; i < 3; i++) {
        uint8_t block = WRITE_ORDER[i];
        if (cardfmt::isSectorTrailer(block)) return result(CardStatus::PROVISION_FAILED, uid);  // never reached
        status = mfrc.MIFARE_Write(block, data + (block - cardfmt::BLOCK_HEADER) * cardfmt::BLOCK_SIZE,
                                   cardfmt::BLOCK_SIZE);
        if (status != MFRC522::STATUS_OK) {
            Serial.printf("[RFID] Card %s: write of block %u failed (%s). Hold the card still and tap again.\n",
                          uid.c_str(), block, String(MFRC522::GetStatusCodeName(status)).c_str());
            return result(CardStatus::PROVISION_FAILED, uid);
        }
    }

    // Read back and compare, so a weak write is reported instead of discovered at the gate.
    for (uint8_t i = 0; i < 3; i++) {
        uint8_t buffer[18];
        uint8_t size = sizeof(buffer);
        status = mfrc.MIFARE_Read(DATA_BLOCKS[i], buffer, &size);
        if (status != MFRC522::STATUS_OK ||
            memcmp(buffer, data + i * cardfmt::BLOCK_SIZE, cardfmt::BLOCK_SIZE) != 0) {
            Serial.printf("[RFID] Card %s: verify of block %u failed. Tap again.\n", uid.c_str(), DATA_BLOCKS[i]);
            return result(CardStatus::PROVISION_FAILED, uid);
        }
    }

    bool wiped = mode == Mode::WIPE;
    cancel();
    return result(wiped ? CardStatus::WIPE_OK : CardStatus::PROVISION_OK, uid);
}

#endif  // HAS_RFID
