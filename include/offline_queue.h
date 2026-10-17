#pragma once
#ifndef HOSTELHIVE_OFFLINE_QUEUE_H
#define HOSTELHIVE_OFFLINE_QUEUE_H

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "api_client.h"

struct QueuedScan {
    String studentId;
    String nodeId;
    String direction;
    String timestampIso;
};

class OfflineQueue {
public:
    OfflineQueue();
    bool init();
    bool enqueueScan(const String& studentId, const String& nodeId, const String& direction, const String& timestampIso);
    int flushQueue(ApiClient& client);  // Flushes cached records to backend
    int getQueueCount();
    void clearQueue();

private:
    bool isFsMounted;
    int currentCount;
    void loadCount();
};

extern OfflineQueue offlineQueue;

#endif // HOSTELHIVE_OFFLINE_QUEUE_H
