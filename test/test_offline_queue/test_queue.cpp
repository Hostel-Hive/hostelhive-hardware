#include <unity.h>
#include "../test_common.h"
#include <ArduinoJson.h>

void test_json_payload_serialization(void) {
    StaticJsonDocument<256> doc;
    doc["student_id"] = "HH-STU-2021-CS-045";
    doc["node_id"] = "GATE_ENTRY_01";
    doc["direction"] = "IN";
    doc["timestamp"] = "2026-10-15T20:00:00+05:30";
    doc["offline_synced"] = true;

    std::string output;
    size_t bytes = serializeJson(doc, output);

    TEST_ASSERT_GREATER_THAN(0, bytes);
    TEST_ASSERT_TRUE(output.find("HH-STU-2021-CS-045") != std::string::npos);
    TEST_ASSERT_TRUE(output.find("\"direction\":\"IN\"") != std::string::npos);
}

void test_json_array_capacity_limit(void) {
    DynamicJsonDocument doc(1024);
    JsonArray array = doc.to<JsonArray>();

    for (int i = 0; i < 5; i++) {
        JsonObject item = array.createNestedObject();
        item["id"] = i;
    }

    TEST_ASSERT_EQUAL_INT(5, array.size());
}

int runQueueTests(void) {
    UNITY_BEGIN();
    RUN_TEST(test_json_payload_serialization);
    RUN_TEST(test_json_array_capacity_limit);
    return UNITY_END();
}

#ifdef ARDUINO
void setup() {
    delay(2000);
    runQueueTests();
}
void loop() {}
#else
int main(int argc, char **argv) {
    return runQueueTests();
}
#endif
