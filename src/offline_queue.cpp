#include "offline_queue.h"
#include "config.h"

OfflineQueue offlineQueue;

OfflineQueue::OfflineQueue()
    : isFsMounted(false), currentCount(0) {}

bool OfflineQueue::init() {
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
    if (!isFsMounted || !LittleFS.exists(QUEUE_STORAGE_PATH)) {
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

int OfflineQueue::flushQueue(ApiClient& client) {
    if (!isFsMounted || !LittleFS.exists(QUEUE_STORAGE_PATH)) {
        return 0;
    }

    File readFile = LittleFS.open(QUEUE_STORAGE_PATH, "r");
    if (!readFile) return 0;

    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, readFile);
    readFile.close();

    if (error || !doc.is<JsonArray>()) {
        clearQueue();
        return 0;
    }

    JsonArray array = doc.as<JsonArray>();
    int originalCount = array.size();
    if (originalCount == 0) return 0;

    Serial.printf("[STORAGE] Flushing %d offline scans to server...\n", originalCount);
    int flushedCount = 0;

    // Remaining items if transmission partially fails
    DynamicJsonDocument remainingDoc(4096);
    JsonArray remainingArray = remainingDoc.to<JsonArray>();

    for (JsonObject obj : array) {
        String studentId = obj["student_id"].as<String>();
        String nodeId = obj["node_id"].as<String>();
        String direction = obj["direction"].as<String>();
        String timestamp = obj["timestamp"].as<String>();

        ScanResponse resp = client.dispatchScan(studentId, nodeId, direction, timestamp, true);
        if (resp.success) {
            flushedCount++;
            delay(50); // Pacing delay to avoid backend storm
        } else {
            // Keep in queue for next flush attempt
            remainingArray.add(obj);
        }
    }

    // Save remaining items back
    if (remainingArray.size() > 0) {
        File writeFile = LittleFS.open(QUEUE_STORAGE_PATH, "w");
        if (writeFile) {
            serializeJson(remainingDoc, writeFile);
            writeFile.close();
        }
        currentCount = remainingArray.size();
    } else {
        clearQueue();
    }

    Serial.printf("[STORAGE] Flush complete. Dispatched: %d | Remaining in buffer: %d\n", flushedCount, currentCount);
    return flushedCount;
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
