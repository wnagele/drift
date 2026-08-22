#include <unity.h>
#include <Arduino.h>
#include <ArduinoJson.h>

#include "status.h"
#include "../support/fixtures.h"

void setUp() {
    // Drive the module into a known state: two process cycles settle both
    // flags to false regardless of what a previous test left behind.
    status_process();
    status_process();
}

static JsonDocument get_status() {
    JsonDocument doc;
    TEST_ASSERT_FALSE(deserializeJson(doc, status_get()));
    return doc;
}

void test_initially_false() {
    JsonDocument doc = get_status();
    TEST_ASSERT_FALSE(doc["telemetry"]);
    TEST_ASSERT_FALSE(doc["gnss"]);
}

void test_flags_after_receive_and_process() {
    status_telemetry_rcvd();
    status_gnss_rcvd();
    status_process();
    JsonDocument doc = get_status();
    TEST_ASSERT_TRUE(doc["telemetry"]);
    TEST_ASSERT_TRUE(doc["gnss"]);
}

void test_counters_reset_each_cycle() {
    status_telemetry_rcvd();
    status_gnss_rcvd();
    status_process();
    status_process();
    JsonDocument doc = get_status();
    TEST_ASSERT_FALSE(doc["telemetry"]);
    TEST_ASSERT_FALSE(doc["gnss"]);
}

void test_matches_shared_fixture() {
    // Contract: test/fixtures/api/status.json (shared with the dash tests).
    // Asymmetric on purpose - with both flags identical, a telemetry/gnss
    // swap in status_get() (or in the dash) is undetectable.
    status_telemetry_rcvd();   // telemetry arrived, no GNSS yet
    status_process();
    JsonDocument fixture;
    std::string raw = fixture_read("api/status.json");
    TEST_ASSERT_FALSE(raw.empty());
    TEST_ASSERT_FALSE(deserializeJson(fixture, raw));
    JsonDocument doc = get_status();
    TEST_ASSERT_EQUAL_STRING(fixture["type"], doc["type"]);
    TEST_ASSERT_EQUAL(fixture["telemetry"].as<bool>(), doc["telemetry"].as<bool>());
    TEST_ASSERT_EQUAL(fixture["gnss"].as<bool>(), doc["gnss"].as<bool>());
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_initially_false);
    RUN_TEST(test_flags_after_receive_and_process);
    RUN_TEST(test_counters_reset_each_cycle);
    RUN_TEST(test_matches_shared_fixture);
    return UNITY_END();
}
