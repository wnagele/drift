#include <unity.h>
#include <ArduinoFake.h>
#include <ArduinoJson.h>
#include <map>
#include <string>

#include "config.h"
#include "http_api.h"
#include "../support/fixtures.h"

using namespace fakeit;

// The dash blob is injected (the native build does not link the generated
// dash.cpp). Distinct magic bytes so a wrong blob/length is detectable.
static const uint8_t FAKE_DASH[] = {
    0x1F, 0x8B, 0x08, 0x00, 0xD4, 0x51, 0x0D, 0x5A,
    0x00, 0x03, 0x4B, 0x4C, 0x4A, 0x06, 0x00, 0x62, 0x67, 0xA9, 0x6D,
};

// In-memory ConfigStorage backend (same pattern as test_config).
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
    http_api_init(FAKE_DASH, sizeof(FAKE_DASH));
    config_init(&mem_storage, "DRIFT_ABCD");
}

void test_get_root_serves_the_dash_blob_gzipped() {
    HttpApiResponse r = http_api_get(HTTP_API_ROOT);
    TEST_ASSERT_TRUE(r.handled);
    TEST_ASSERT_EQUAL(200, r.status);
    TEST_ASSERT_EQUAL_STRING("text/html", r.content_type);
    TEST_ASSERT_TRUE(r.gzip);
    TEST_ASSERT_EQUAL(r.body, FAKE_DASH);
    TEST_ASSERT_EQUAL(sizeof(FAKE_DASH), r.body_len);
    // The pinned route paths are the ones net.cpp registers.
    TEST_ASSERT_EQUAL_STRING("/", HTTP_API_ROOT);
    TEST_ASSERT_EQUAL_STRING("/api/config", HTTP_API_CONFIG);
    TEST_ASSERT_EQUAL_STRING("/debug/info", HTTP_API_DEBUG_INFO);
}

void test_get_api_config_matches_shared_fixture() {
    // Contract: GET /api/config returns the shared fixture document
    // (test/fixtures/api/config.json) - the same bytes the dash tests
    // replay. Firmware and dash are finally asserted against one source.
    std::string raw = fixture_read("api/config.json");
    TEST_ASSERT_FALSE(raw.empty());
    config_save(String(raw.c_str()));

    HttpApiResponse r = http_api_get(HTTP_API_CONFIG);
    TEST_ASSERT_TRUE(r.handled);
    TEST_ASSERT_EQUAL(200, r.status);
    TEST_ASSERT_EQUAL_STRING("application/json", r.content_type);
    TEST_ASSERT_FALSE(r.gzip);

    JsonDocument fixture;
    TEST_ASSERT_FALSE(deserializeJson(fixture, raw));
    JsonDocument body;
    TEST_ASSERT_FALSE(deserializeJson(body, r.body_text));
    TEST_ASSERT(fixture == body);
}

void test_get_debug_info_shape() {
    HttpApiResponse r = http_api_get(HTTP_API_DEBUG_INFO);
    TEST_ASSERT_TRUE(r.handled);
    TEST_ASSERT_EQUAL(200, r.status);
    TEST_ASSERT_EQUAL_STRING("application/json", r.content_type);
    JsonDocument body;
    TEST_ASSERT_FALSE(deserializeJson(body, r.body_text));
    TEST_ASSERT(body["version"].isNull() || body["version"].is<const char *>());
    TEST_ASSERT(body["git_ref"].isNull() || body["git_ref"].is<const char *>());
    TEST_ASSERT(body["build_time"].isNull() || body["build_time"].is<const char *>());
}

void test_get_unknown_path_is_not_handled() {
    TEST_ASSERT_FALSE(http_api_get("/nope").handled);
    TEST_ASSERT_FALSE(http_api_get("/api").handled);
    TEST_ASSERT_FALSE(http_api_get("/api/config/extra").handled);
}

void test_post_config_saves_and_restarts() {
    std::string raw = fixture_read("api/config.json");
    TEST_ASSERT_FALSE(raw.empty());

    HttpApiResponse r = http_api_post_config(raw.c_str());
    TEST_ASSERT_TRUE(r.handled);
    TEST_ASSERT_EQUAL(200, r.status);
    TEST_ASSERT_TRUE(r.restart_after);

    // The body really was saved.
    JsonDocument fixture;
    TEST_ASSERT_FALSE(deserializeJson(fixture, raw));
    TEST_ASSERT_EQUAL_STRING(fixture["wifi"]["ssid"], config_wifi_ssid().c_str());
    TEST_ASSERT_EQUAL_STRING(fixture["dri"]["op_id"], config_dri_op_id().c_str());
}

void test_post_config_without_body_is_rejected() {
    // A bodiless POST gets 400 (the request-handler side of the split) and,
    // crucially, does not restart the device.
    HttpApiResponse r = http_api_post_config(NULL);
    TEST_ASSERT_TRUE(r.handled);
    TEST_ASSERT_EQUAL(400, r.status);
    TEST_ASSERT_FALSE(r.restart_after);
    TEST_ASSERT_EQUAL_STRING("DRIFT_ABCD", config_wifi_ssid().c_str());  // untouched
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_get_root_serves_the_dash_blob_gzipped);
    RUN_TEST(test_get_api_config_matches_shared_fixture);
    RUN_TEST(test_get_debug_info_shape);
    RUN_TEST(test_get_unknown_path_is_not_handled);
    RUN_TEST(test_post_config_saves_and_restarts);
    RUN_TEST(test_post_config_without_body_is_rejected);
    return UNITY_END();
}
