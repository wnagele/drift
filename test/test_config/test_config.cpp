#include <unity.h>
#include <ArduinoFake.h>
#include <ArduinoJson.h>
#include <map>
#include <string>
#include <string.h>

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
    // config_save() logs to Serial on every rejection.
    When(OverloadedMethod(ArduinoFake(Serial), println, size_t(const char *))).AlwaysReturn(1);
}

// --- Shared-fixture helpers ---------------------------------------------------

// Every string field of the posted document, bound to its accessor. Under the
// complete-document contract all of them are mandatory, so this one table
// drives the round-trip assertions and the per-field rejection tests alike -
// a field added to the API without a test lands here or nowhere.
static struct Binding {
    const char *section;
    const char *key;
    String (*get)();
} const bindings[] = {
    { "wifi", "ssid", config_wifi_ssid },
    { "wifi", "password", config_wifi_password },
    { "dri", "region", config_dri_region },
    { "dri", "ua_id", config_dri_ua_id },
    { "dri", "ua_desc", config_dri_ua_desc },
    { "dri", "op_id", config_dri_op_id },
    { "dri", "op_secret", config_dri_op_secret },
};

// The boolean fields, which are equally mandatory but read back as bools.
static struct BoolBinding {
    const char *key;
    bool (*get)();
} const bool_bindings[] = {
    { "bt5_enabled", config_bt5_enabled },
    { "wifi_beacon_enabled", config_wifi_beacon_enabled },
    { "wifi_nan_enabled", config_wifi_nan_enabled },
};

static void load_fixture(JsonDocument &doc) {
    // Contract: test/fixtures/api/config.json (shared with the dash tests).
    std::string raw = fixture_read("api/config.json");
    TEST_ASSERT_FALSE(raw.empty());
    TEST_ASSERT_FALSE(deserializeJson(doc, raw));
}

static bool save(JsonDocument &doc) {
    String buf;
    serializeJson(doc, buf);
    return config_save(buf);
}

// The baseline every rejection test starts from: a known-good complete
// document, so "nothing was written" can be checked against known values
// rather than against defaults.
static void save_fixture_baseline() {
    JsonDocument doc;
    load_fixture(doc);
    TEST_ASSERT_TRUE(save(doc));
}

static void assert_all_fields_match_fixture(const std::string &label) {
    JsonDocument fixture;
    load_fixture(fixture);
    for (const Binding &b : bindings) {
        std::string msg = label + ": " + b.key;
        TEST_ASSERT_EQUAL_STRING_MESSAGE(fixture[b.section][b.key], b.get().c_str(), msg.c_str());
    }
    for (const BoolBinding &b : bool_bindings) {
        std::string msg = label + ": " + b.key;
        TEST_ASSERT_EQUAL_INT_MESSAGE(fixture["dri"][b.key].as<bool>(), b.get(), msg.c_str());
    }
}

// --- Defaults -----------------------------------------------------------------

void test_init_creates_defaults() {
    config_init(&mem_storage, "DRIFT_ABCD");
    TEST_ASSERT_EQUAL_STRING("DRIFT_ABCD", config_wifi_ssid().c_str());
    TEST_ASSERT_EQUAL_STRING("", config_wifi_password().c_str());
    TEST_ASSERT_EQUAL_STRING("", config_dri_ua_id().c_str());
    TEST_ASSERT_EQUAL_STRING("", config_dri_ua_desc().c_str());
    TEST_ASSERT_EQUAL_STRING("", config_dri_op_id().c_str());
    // The verification code defaults to empty, which is also the steady
    // state for the many registries that issue none at all.
    TEST_ASSERT_EQUAL_STRING("", config_dri_op_secret().c_str());
    // Region has no safe default - DRIFT cannot guess the jurisdiction - so
    // an unconfigured device claims nothing. Same value the dash's "Other -
    // no regional rules" choice posts.
    TEST_ASSERT_EQUAL_STRING("", config_dri_region().c_str());
    // The BLE5 transport defaults to on.
    TEST_ASSERT_TRUE(config_bt5_enabled());
    // The Wi-Fi Beacon transport defaults to on.
    TEST_ASSERT_TRUE(config_wifi_beacon_enabled());
    // The Wi-Fi NAN transport defaults to off.
    TEST_ASSERT_FALSE(config_wifi_nan_enabled());
}

void test_init_preserves_existing_values() {
    kv["wifi_ssid"] = "KEEPME";
    kv["dri_region"] = "EU";
    kv["bt5_enabled"] = "0";
    kv["wifi_beacon"] = "0";
    kv["wifi_nan"] = "1";
    config_init(&mem_storage, "DRIFT_ABCD");
    TEST_ASSERT_EQUAL_STRING("KEEPME", config_wifi_ssid().c_str());
    TEST_ASSERT_EQUAL_STRING("EU", config_dri_region().c_str());
    TEST_ASSERT_FALSE(config_bt5_enabled());
    TEST_ASSERT_FALSE(config_wifi_beacon_enabled());
    TEST_ASSERT_TRUE(config_wifi_nan_enabled());
}

// --- Transport enables ---------------------------------------------------------

// Each transport flag round-trips through a complete document, and
// config_get() emits the stored state as a JSON boolean. Driven off the
// bool_bindings table so a new transport cannot be added without coverage.
void test_transport_flags_roundtrip() {
    for (const BoolBinding &b : bool_bindings) {
        for (int value = 0; value <= 1; value++) {
            kv.clear();
            config_init(&mem_storage, "DRIFT_ABCD");
            JsonDocument doc;
            load_fixture(doc);
            doc["dri"][b.key] = (bool)value;
            TEST_ASSERT_TRUE(save(doc));

            std::string label = std::string(b.key) + (value ? " = true" : " = false");
            TEST_ASSERT_EQUAL_INT_MESSAGE(value, b.get(), label.c_str());

            JsonDocument got;
            TEST_ASSERT_FALSE(deserializeJson(got, config_get()));
            TEST_ASSERT_EQUAL_INT_MESSAGE(value, got["dri"][b.key].as<bool>(), label.c_str());
        }
    }
}

// A boolean that is absent, null or not a boolean is a rejection, not a
// fallback to the default. This replaces the old "absent keeps the default"
// rule: that rule existed only to stop a *partial* POST from silently
// disabling (or enabling) a broadcast, and a partial POST is now refused
// outright, which is a louder and less guessy answer to the same risk.
void test_non_boolean_transport_flag_is_rejected() {
    for (const BoolBinding &b : bool_bindings) {
        for (int mode = 0; mode <= 3; mode++) {
            kv.clear();
            config_init(&mem_storage, "DRIFT_ABCD");
            save_fixture_baseline();

            JsonDocument doc;
            load_fixture(doc);
            switch (mode) {
                case 0: doc["dri"].remove(b.key); break;
                case 1: doc["dri"][b.key] = nullptr; break;
                case 2: doc["dri"][b.key] = "yes"; break;
                case 3: doc["dri"][b.key] = 1; break;
            }
            std::string label = std::string(b.key) + " mode " + std::to_string(mode);
            TEST_ASSERT_FALSE_MESSAGE(save(doc), label.c_str());
            assert_all_fields_match_fixture(label);
        }
    }
}

// --- Region --------------------------------------------------------------------

void test_known_regions_roundtrip() {
    // "" is a real choice, not just the default: it is the dash's "Other -
    // no regional rules" option, correct for every jurisdiction that
    // mandates nothing. It therefore has to survive a round trip like any
    // other value.
    const char *const regions[] = {"", "US", "EU", "UK"};
    for (const char *region : regions) {
        kv.clear();
        config_init(&mem_storage, "DRIFT_ABCD");
        JsonDocument doc;
        load_fixture(doc);
        doc["dri"]["region"] = region;
        TEST_ASSERT_TRUE_MESSAGE(save(doc), region[0] ? region : "(empty)");
        TEST_ASSERT_EQUAL_STRING(region, config_dri_region().c_str());

        JsonDocument got;
        TEST_ASSERT_FALSE(deserializeJson(got, config_get()));
        TEST_ASSERT_EQUAL_STRING(region, got["dri"]["region"]);
    }
}

// The one region rule the firmware enforces. It exists so an unservable
// value cannot be persisted - not so anything can branch on the region,
// which nothing does: every actual region rule (required fields, the EU/UK
// registration-number format) lives in the dash.
void test_unknown_region_is_rejected() {
    // "us" pins that the match is case-sensitive, and "JP"/"CN" are the two
    // regions deliberately out of scope, so a future contributor wiring them
    // up trips this test. Note "" is *not* here - it is the "Other" choice
    // (test_known_regions_roundtrip), so only a non-empty unknown is an
    // error. " US" pins that no trimming happens either.
    const char *const rejected[] = {"us", "eu", "XX", "JP", "CN", "USA", " US"};
    for (const char *region : rejected) {
        kv.clear();
        config_init(&mem_storage, "DRIFT_ABCD");
        save_fixture_baseline();

        JsonDocument doc;
        load_fixture(doc);
        doc["dri"]["region"] = region;
        std::string label = std::string("region '") + region + "'";
        TEST_ASSERT_FALSE_MESSAGE(save(doc), label.c_str());
        assert_all_fields_match_fixture(label);
    }
}

// --- The complete-document contract ---------------------------------------------

void test_save_get_roundtrip_matches_shared_fixture() {
    JsonDocument fixture;
    load_fixture(fixture);

    config_init(&mem_storage, "DRIFT_ABCD");
    save_fixture_baseline();
    assert_all_fields_match_fixture("fixture");

    // config_get() must return the same document.
    JsonDocument got;
    TEST_ASSERT_FALSE(deserializeJson(got, config_get()));
    TEST_ASSERT(fixture == got);
}

void test_malformed_json_is_rejected_and_keeps_existing_values() {
    config_init(&mem_storage, "DRIFT_ABCD");
    save_fixture_baseline();
    TEST_ASSERT_FALSE(config_save("this is not json {{"));
    assert_all_fields_match_fixture("malformed");
}

// Previously this stored the literal string "null" into every field - an
// empty POST wiped the identity and the SSID and still answered 200. Now it
// is a rejection that writes nothing.
void test_empty_document_is_rejected() {
    config_init(&mem_storage, "DRIFT_ABCD");
    save_fixture_baseline();
    TEST_ASSERT_FALSE(config_save("{}"));
    assert_all_fields_match_fixture("empty document");
}

void test_missing_section_is_rejected() {
    const char *const sections[] = {"wifi", "dri"};
    for (const char *section : sections) {
        kv.clear();
        config_init(&mem_storage, "DRIFT_ABCD");
        save_fixture_baseline();

        JsonDocument doc;
        load_fixture(doc);
        doc.remove(section);
        std::string label = std::string("missing section: ") + section;
        TEST_ASSERT_FALSE_MESSAGE(save(doc), label.c_str());
        assert_all_fields_match_fixture(label);
    }
}

// Per-field: a key missing from the posted document, explicitly null, or of
// the wrong type is a rejection that writes *nothing at all* - not even the
// fields that were valid. That last part is the point of validating the
// whole document before the first putString, and it is what the assertion
// over every other field checks.
void test_missing_null_or_wrong_typed_field_is_rejected_per_field() {
    for (int mode = 0; mode <= 2; mode++) {
        for (const Binding &field : bindings) {
            kv.clear();
            config_init(&mem_storage, "DRIFT_ABCD");
            save_fixture_baseline();

            JsonDocument doc;
            load_fixture(doc);
            const char *mode_label = "";
            switch (mode) {
                case 0:
                    doc[field.section].remove(field.key);
                    mode_label = "missing: ";
                    break;
                case 1:
                    doc[field.section][field.key] = nullptr;
                    mode_label = "null: ";
                    break;
                case 2:
                    // A non-string where a string is required. Rejecting
                    // rather than coercing is what keeps a JSON null from
                    // being persisted as the string "null".
                    doc[field.section][field.key] = 42;
                    mode_label = "wrong type: ";
                    break;
            }
            std::string label = std::string(mode_label) + field.key;
            TEST_ASSERT_FALSE_MESSAGE(save(doc), label.c_str());
            assert_all_fields_match_fixture(label);
        }
    }
}

// Empty strings are legal for every field: an empty password means an open
// AP, an empty op_id is the US case (no operator registration in 14 CFR
// 89.315), an empty ua_desc means no Self-ID message, and an empty region is
// the "Other - no regional rules" choice. Only absence and wrong types are
// errors.
void test_empty_strings_are_accepted() {
    for (const Binding &field : bindings) {
        kv.clear();
        config_init(&mem_storage, "DRIFT_ABCD");
        JsonDocument doc;
        load_fixture(doc);
        doc[field.section][field.key] = "";
        TEST_ASSERT_TRUE_MESSAGE(save(doc), field.key);
        TEST_ASSERT_EQUAL_STRING_MESSAGE("", field.get().c_str(), field.key);
    }
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_init_creates_defaults);
    RUN_TEST(test_init_preserves_existing_values);
    RUN_TEST(test_transport_flags_roundtrip);
    RUN_TEST(test_non_boolean_transport_flag_is_rejected);
    RUN_TEST(test_known_regions_roundtrip);
    RUN_TEST(test_unknown_region_is_rejected);
    RUN_TEST(test_save_get_roundtrip_matches_shared_fixture);
    RUN_TEST(test_malformed_json_is_rejected_and_keeps_existing_values);
    RUN_TEST(test_empty_document_is_rejected);
    RUN_TEST(test_missing_section_is_rejected);
    RUN_TEST(test_missing_null_or_wrong_typed_field_is_rejected_per_field);
    RUN_TEST(test_empty_strings_are_accepted);
    return UNITY_END();
}
