#include "offline_queue.h"
#include "config.h"

OfflineQueue offlineQueue;

OfflineQueue::OfflineQueue()
    : isFsMounted(false), currentCount(0) {}

bool OfflineQueue::init() {
    // Mount LittleFS file system
    if (!LittleFS.begin(true)) {
        Serial.println("[STORAGE] LittleFS mount failed!");
        isFsMounted = false;
        return false;
    }

    isFsMounted = true;
    Serial.println("[STORAGE] LittleFS mounted successfully (FR015 Offline Buffer).");
    loadCount();
    Serial.printf("[STORAGE] Existing offline cached scans count: %d\n", currentCount);
    return true;
}

void OfflineQueue::loadCount() {
    if (!isFsMounted) {
        currentCount = 0;
        return;
    }

    if (!LittleFS.exists(QUEUE_STORAGE_PATH)) {
        currentCount = 0;
        return;
    }

    File file = LittleFS.open(QUEUE_STORAGE_PATH, "r");
    if (!file) {
        currentCount = 0;
        return;
    }

    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (!error && doc.is<JsonArray>()) {
        currentCount = doc.as<JsonArray>().size();
    } else {
        currentCount = 0;
    }
}

bool OfflineQueue::enqueueScan(const String& studentId, const String& nodeId, const String& direction, const String& timestampIso) {
    if (!isFsMounted) {
        Serial.println("[STORAGE] Error: Cannot enqueue, LittleFS not mounted.");
        return false;
    }

    DynamicJsonDocument doc(4096);

    if (LittleFS.exists(QUEUE_STORAGE_PATH)) {
        File readFile = LittleFS.open(QUEUE_STORAGE_PATH, "r");
        if (readFile) {
            deserializeJson(doc, readFile);
            readFile.close();
        }
    }

    JsonArray array = doc.as<JsonArray>();
    if (array.isNull()) {
        array = doc.to<JsonArray>();
    }

    if (array.size() >= MAX_OFFLINE_CACHE_ENTRIES) {
        Serial.println("[STORAGE] Warning: Cache full, dropping oldest scan.");
        array.remove(0);
    }

    JsonObject obj = array.createNestedObject();
    obj["student_id"] = studentId;
    obj["node_id"] = nodeId;
    obj["direction"] = direction;
    obj["timestamp"] = timestampIso;

    File writeFile = LittleFS.open(QUEUE_STORAGE_PATH, "w");
    if (!writeFile) {
        Serial.println("[STORAGE] Failed to open queue file for writing.");
        return false;
    }

    serializeJson(doc, writeFile);
    writeFile.close();

    currentCount = array.size();
    Serial.printf("[STORAGE] Scan buffered offline. New count: %d\n", currentCount);
    return true;
}

int OfflineQueue::getQueueCount() {
    return currentCount;
}

void OfflineQueue::clearQueue() {
    if (!isFsMounted) return;
    if (LittleFS.exists(QUEUE_STORAGE_PATH)) {
        LittleFS.remove(QUEUE_STORAGE_PATH);
    }
    currentCount = 0;
    Serial.println("[STORAGE] Offline queue cleared.");
}
