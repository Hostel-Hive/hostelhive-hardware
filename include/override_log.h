#pragma once
#ifndef HOSTELHIVE_OVERRIDE_LOG_H
#define HOSTELHIVE_OVERRIDE_LOG_H

#include <Arduino.h>
#include <LittleFS.h>

// Append-only LittleFS record of every manual override unlock, so physical
// overrides leave a trail that staff can read over USB serial ("overrides").
// The backend has no override endpoint yet; until it does this is the record.
class OverrideLog {
public:
    OverrideLog();
    bool init();
    // Records one override and returns its sequence number (0 if not stored).
    unsigned long record(const String& timestampIso, bool timeSynced, bool wifiConnected);
    unsigned long totalCount() const { return sequence; }
    void dump(Stream& out);

private:
    bool mounted;
    unsigned long sequence;
    void loadSequence();
};

extern OverrideLog overrideLog;

#endif // HOSTELHIVE_OVERRIDE_LOG_H
