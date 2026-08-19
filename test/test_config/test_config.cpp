#include <unity.h>
#include <ArduinoFake.h>
#include <ArduinoJson.h>
#include <map>
#include <string>

#include "config.h"
#include "../support/fixtures.h"

using namespace fakeit;

// In-memory ConfigStorage backend.
static std::map<std::string, std::string> kv;

static bool mem_isKey(const char *key) {
    return kv.find(key) != kv.end();
}

static String mem_getString(const char *key) {
    auto it = kv.find(key);
    return it == kv.end() ? String() : String(it->second.c_str());
}

static void mem_putString(const char *key, const String &value) {
    kv[key] = std::string(value.c_str());
}

static const ConfigStorage mem_storage = { mem_isKey, mem_getString, mem_putString };

void setUp() {
    kv.clear();
    ArduinoFakeReset();
    // config_save() logs to Serial on malformed JSON.
    When(OverloadedMethod(ArduinoFake(Serial), println, size_t(const char *))).AlwaysReturn(1);
}

void test_init_creates_defaults() {
    config_init(&mem_storage, "DRIFT_ABCD");
    TEST_ASSERT_EQUAL_STRING("DRIFT_ABCD", config_wifi_ssid().c_str());
    TEST_ASSERT_EQUAL_STRING("", config_wifi_password().c_str());
    TEST_ASSERT_EQUAL_STRING("", config_dri_ua_id().c_str());
    TEST_ASSERT_EQUAL_STRING("", config_dri_ua_desc().c_str());
    TEST_ASSERT_EQUAL_STRING("", config_dri_op_id().c_str());
}

void test_init_preserves_existing_values() {
    kv["wifi_ssid"] = "KEEPME";
    config_init(&mem_storage, "DRIFT_ABCD");
    TEST_ASSERT_EQUAL_STRING("KEEPME", config_wifi_ssid().c_str());
}

void test_save_get_roundtrip_matches_shared_fixture() {
    // Contract: test/fixtures/api/config.json (shared with the dash tests).
    std::string raw = fixture_read("api/config.json");
    TEST_ASSERT_FALSE(raw.empty());

    config_init(&mem_storage, "DRIFT_ABCD");
    config_save(String(raw.c_str()));

    JsonDocument fixture;
    TEST_ASSERT_FALSE(deserializeJson(fixture, raw));
    TEST_ASSERT_EQUAL_STRING(fixture["wifi"]["ssid"], config_wifi_ssid().c_str());
    TEST_ASSERT_EQUAL_STRING(fixture["wifi"]["password"], config_wifi_password().c_str());
    TEST_ASSERT_EQUAL_STRING(fixture["dri"]["ua_id"], config_dri_ua_id().c_str());
    TEST_ASSERT_EQUAL_STRING(fixture["dri"]["ua_desc"], config_dri_ua_desc().c_str());
    TEST_ASSERT_EQUAL_STRING(fixture["dri"]["op_id"], config_dri_op_id().c_str());

    // config_get() must return the same document.
    JsonDocument got;
    TEST_ASSERT_FALSE(deserializeJson(got, config_get()));
    TEST_ASSERT(fixture == got);
}

void test_malformed_json_keeps_existing_values() {
    config_init(&mem_storage, "DRIFT_ABCD");
    config_save("{\"wifi\":{\"ssid\":\"GOOD\"}}");
    config_save("this is not json {{");
    TEST_ASSERT_EQUAL_STRING("GOOD", config_wifi_ssid().c_str());
}

void test_missing_sections_currently_store_null() {
    // Characterization of current behavior: missing sections are read as
    // null variants and stored as the string "null".
    config_init(&mem_storage, "DRIFT_ABCD");
    config_save("{}");
    TEST_ASSERT_EQUAL_STRING("null", config_wifi_ssid().c_str());
    TEST_ASSERT_EQUAL_STRING("null", config_dri_op_id().c_str());
}

// Config accessors bound to their JSON fields, for the per-field tests below.
static struct Binding {
    const char *section;
    const char *key;
    String (*get)();
} const bindings[] = {
    { "wifi", "ssid", config_wifi_ssid },
    { "wifi", "password", config_wifi_password },
    { "dri", "ua_id", config_dri_ua_id },
    { "dri", "ua_desc", config_dri_ua_desc },
    { "dri", "op_id", config_dri_op_id },
};

static void save_with_field(const Binding &field, int mode) {
    // Re-save the shared fixture document with one field removed (mode 0)
    // or set to an explicit JSON null (mode 1).
    std::string raw = fixture_read("api/config.json");
    TEST_ASSERT_FALSE(raw.empty());
    JsonDocument doc;
    TEST_ASSERT_FALSE(deserializeJson(doc, raw));
    if (mode == 0)
        doc[field.section].remove(field.key);
    else
        doc[field.section][field.key] = nullptr;
    String mutated;
    serializeJson(doc, mutated);
    config_save(mutated);
}

void test_missing_or_null_field_stores_null_per_field() {
    // Per-field characterization (the whole-{} case is pinned above): a key
    // missing from the posted document - or explicitly null - converts to
    // the string "null" independently for every field, while all the other
    // fields keep their posted values. Pins each String conversion and each
    // putString against a dropped or mis-keyed write.
    std::string raw = fixture_read("api/config.json");
    TEST_ASSERT_FALSE(raw.empty());
    JsonDocument fixture;
    TEST_ASSERT_FALSE(deserializeJson(fixture, raw));

    for (int mode = 0; mode <= 1; mode++) {
        for (const Binding &field : bindings) {
            kv.clear();
            config_init(&mem_storage, "DRIFT_ABCD");
            save_with_field(field, mode);

            for (const Binding &other : bindings) {
                std::string label = (mode == 0 ? "missing: " : "null: ");
                label += other.key;
                if (&other == &field)
                    TEST_ASSERT_EQUAL_STRING_MESSAGE("null", other.get().c_str(), label.c_str());
                else
                    TEST_ASSERT_EQUAL_STRING_MESSAGE(fixture[other.section][other.key],
                                                     other.get().c_str(), label.c_str());
            }
        }
    }
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_init_creates_defaults);
    RUN_TEST(test_init_preserves_existing_values);
    RUN_TEST(test_save_get_roundtrip_matches_shared_fixture);
    RUN_TEST(test_malformed_json_keeps_existing_values);
    RUN_TEST(test_missing_sections_currently_store_null);
    RUN_TEST(test_missing_or_null_field_stores_null_per_field);
    return UNITY_END();
}
