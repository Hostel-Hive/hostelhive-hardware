#pragma once
#ifndef HOSTELHIVE_RFID_READER_H
#define HOSTELHIVE_RFID_READER_H

#include <Arduino.h>
#include "config.h"

#if HAS_RFID

// RC522 (MFRC522) reader for MIFARE Classic cards that carry a HostelHive pass
// in the layout described in card_format.h.
enum class CardStatus {
    NONE,               // no new card this poll
    PASS,               // card holds a valid pass (payload set)
    NOT_REGISTERED,     // blank card, other data, or a sector key we don't know
    CORRUPT,            // HostelHive header but bad CRC/version: re-provision it
    UNSUPPORTED,        // not a MIFARE Classic card (e.g. bank card, NTAG)
    READ_ERROR,         // card moved away or RF error: tap again
    PROVISION_OK,       // pass written and read back
    WIPE_OK,            // HostelHive data erased
    PROVISION_FAILED,   // write/verify failed; still waiting for a card
    PROVISION_TIMEOUT   // nobody tapped a card in time; write cancelled
};

struct CardRead {
    CardStatus status;
    String payload;  // "HH1:..." only when status == PASS
    String uid;      // card UID as hex, for the log only (never used for access)
};

class RfidReader {
public:
    RfidReader();
    bool init();
    // Call every loop. Reads (or, while armed, writes) at most one card.
    CardRead poll();

    // Arms the next tapped card to receive `pass`. Returns false if the pass is malformed.
    bool armProvision(const String& pass);
    // Arms the next tapped card to have its HostelHive blocks zeroed.
    void armWipe();
    void cancel();
    bool isArmed() const { return mode != Mode::READ; }

    bool isReaderOk() const { return readerOk; }
    uint8_t firmwareVersion() const { return version; }

private:
    enum class Mode { READ, PROVISION, WIPE };

    bool checkReader(bool log);
    CardRead readCard(const String& uid);
    CardRead writeCard(const String& uid);

    Mode mode;
    String pendingPass;
    unsigned long armedAt;
    unsigned long lastHealthCheck;
    bool readerOk;
    uint8_t version;
};

extern RfidReader rfidReader;

#endif  // HAS_RFID

#endif  // HOSTELHIVE_RFID_READER_H
