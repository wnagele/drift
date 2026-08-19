#include <unity.h>
#include <Arduino.h>
#include <ArduinoJson.h>

#include "debug.h"

void test_debug_info_shape() {
    String info = debug_info();
    JsonDocument doc;
    TEST_ASSERT_FALSE(deserializeJson(doc, info));
    TEST_ASSERT_TRUE(doc.containsKey("version"));
    TEST_ASSERT_TRUE(doc.containsKey("git_ref"));
    TEST_ASSERT_TRUE(doc.containsKey("build_time"));
}

void test_debug_info_defaults_null() {
    // Without CI-injected defines, all fields default to null (contract:
    // test/fixtures/api/debug-info.json).
    String info = debug_info();
    JsonDocument doc;
    TEST_ASSERT_FALSE(deserializeJson(doc, info));
    TEST_ASSERT_TRUE(doc["version"].isNull());
    TEST_ASSERT_TRUE(doc["git_ref"].isNull());
    TEST_ASSERT_TRUE(doc["build_time"].isNull());
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_debug_info_shape);
    RUN_TEST(test_debug_info_defaults_null);
    return UNITY_END();
}
