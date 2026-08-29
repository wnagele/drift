#include <unity.h>
#include <Arduino.h>
#include <ArduinoJson.h>

#include "status.h"
#include "txcount.h"
#include "../support/fixtures.h"

void setUp() {
    // Drive the module into a known state: two process cycles settle both
    // flags to false regardless of what a previous test left behind, and a
    // fresh txcount resets every counter and rate.
    status_process();
    status_process();
    txcount_init(true, true, true);
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
    // Before the first window closes every transport reports zero.
    TEST_ASSERT_EQUAL_UINT32(0, doc["tx"]["bt4"]["frames"].as<unsigned long>());
    TEST_ASSERT_EQUAL_UINT32(0, doc["tx"]["bt5"]["messages"].as<unsigned long>());
    TEST_ASSERT_EQUAL_UINT32(0, doc["tx"]["wifi_beacon"]["frames"].as<unsigned long>());
    TEST_ASSERT_EQUAL_UINT32(0, doc["tx"]["wifi_nan"]["messages"].as<unsigned long>());
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

// One transport field of the tx subtree: fixture vs status_get().
static void assert_tx_field(JsonDocument &fixture, JsonDocument &doc,
                            const char *transport, const char *field) {
    char label[48];
    snprintf(label, sizeof(label), "tx.%s.%s", transport, field);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
        fixture["tx"][transport][field].as<unsigned long>(),
        doc["tx"][transport][field].as<unsigned long>(), label);
}

void test_matches_shared_fixture() {
    // Contract: test/fixtures/api/status.json (shared with the dash tests).
    // Asymmetric on purpose - with both flags identical, a telemetry/gnss
    // swap in status_get() (or in the dash) is undetectable.
    status_telemetry_rcvd();   // telemetry arrived, no GNSS yet
    status_process();
    // Seed the tx window with a steady-state bench second: all ten slots of
    // the 10-slot BT4 cycle (an unconfigured Operator ID still broadcasts a
    // zeroed message — only the pack excludes invalid messages), one N=3
    // pack on BT5, five vendor IE refreshes, two NAN action frames (the sync
    // beacon carries no ODID data and is not counted).
    for (int i = 0; i < 10; i++)
        txcount_bt4();
    txcount_bt5(3 + 25 * 3);
    for (int i = 0; i < 5; i++)
        txcount_beacon(3 + 25 * 3);
    txcount_nan(3 + 25 * 3);
    txcount_nan(3 + 25 * 3);
    txcount_sample(1000);
    JsonDocument fixture;
    std::string raw = fixture_read("api/status.json");
    TEST_ASSERT_FALSE(raw.empty());
    TEST_ASSERT_FALSE(deserializeJson(fixture, raw));
    JsonDocument doc = get_status();
    TEST_ASSERT_EQUAL_STRING(fixture["type"], doc["type"]);
    TEST_ASSERT_EQUAL(fixture["telemetry"].as<bool>(), doc["telemetry"].as<bool>());
    TEST_ASSERT_EQUAL(fixture["gnss"].as<bool>(), doc["gnss"].as<bool>());
    static const char *transports[] = { "bt4", "bt5", "wifi_beacon", "wifi_nan" };
    for (const char *transport : transports) {
        assert_tx_field(fixture, doc, transport, "frames");
        assert_tx_field(fixture, doc, transport, "messages");
    }
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_initially_false);
    RUN_TEST(test_flags_after_receive_and_process);
    RUN_TEST(test_counters_reset_each_cycle);
    RUN_TEST(test_matches_shared_fixture);
    return UNITY_END();
}
