#include "override_log.h"
#include "config.h"

OverrideLog overrideLog;

static const char* OVERRIDE_SEQUENCE_PATH = "/override_seq.txt";
static const char* OVERRIDE_LOG_ROTATED_PATH = "/override_log.1.jsonl";

OverrideLog::OverrideLog() : mounted(false), sequence(0) {}

bool OverrideLog::init() {
    if (!LittleFS.begin(true)) {
        Serial.println("[AUDIT] LittleFS mount failed; overrides will only be logged to serial.");
        mounted = false;
        return false;
    }
    mounted = true;
    loadSequence();
    Serial.printf("[AUDIT] Manual overrides recorded on this gate: %lu\n", sequence);
    return true;
}

void OverrideLog::loadSequence() {
    sequence = 0;
    File f = LittleFS.open(OVERRIDE_SEQUENCE_PATH, "r");
    if (!f) return;
    sequence = f.readString().toInt();
    f.close();
}

unsigned long OverrideLog::record(const String& timestampIso, bool timeSynced, bool wifiConnected) {
    sequence++;
    char line[160];
    snprintf(line, sizeof(line), "{\"seq\":%lu,\"node\":\"%s\",\"time\":\"%s\",\"time_synced\":%s,\"uptime_ms\":%lu,\"wifi\":%s}",
             sequence, NODE_ID, timestampIso.c_str(), timeSynced ? "true" : "false", millis(),
             wifiConnected ? "true" : "false");
    Serial.print("[AUDIT] MANUAL OVERRIDE ");
    Serial.println(line);

    if (!mounted) return 0;

    File current = LittleFS.open(OVERRIDE_LOG_PATH, "r");
    if (current) {
        size_t size = current.size();
        current.close();
        if (size >= OVERRIDE_LOG_MAX_BYTES) {
            // Keep one rotated generation; the sequence number reveals any gap.
            LittleFS.remove(OVERRIDE_LOG_ROTATED_PATH);
            LittleFS.rename(OVERRIDE_LOG_PATH, OVERRIDE_LOG_ROTATED_PATH);
        }
    }

    File log = LittleFS.open(OVERRIDE_LOG_PATH, "a");
    if (log) {
        log.println(line);
        log.close();
    }
    File seq = LittleFS.open(OVERRIDE_SEQUENCE_PATH, "w");
    if (seq) {
        seq.print(sequence);
        seq.close();
    }
    return sequence;
}

void OverrideLog::dump(Stream& out) {
    out.printf("[AUDIT] Override count: %lu\n", sequence);
    if (!mounted) return;
    const char* paths[] = {OVERRIDE_LOG_ROTATED_PATH, OVERRIDE_LOG_PATH};
    for (const char* path : paths) {
        File f = LittleFS.open(path, "r");
        if (!f) continue;
        while (f.available()) out.write(f.read());
        f.close();
    }
}
