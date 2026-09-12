#include <unity.h>
#include <ArduinoFake.h>
#include <map>
#include <string>

#include "config.h"
#include "wifi_ap.h"

using namespace fakeit;

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
    // config_save() logs to Serial on every rejection.
    When(OverloadedMethod(ArduinoFake(Serial), println, size_t(const char *))).AlwaysReturn(1);
}

// config_save() takes a *complete* configuration document (config.h), so the
// two AP fields under test have to travel with the rest of the config. Built
// by hand rather than from the shared fixture because what matters here is
// the SSID/password pair, and spelling it out keeps the AP decision visible
// at the call site.
static String config_with_ap(const char *ssid, const char *password) {
    return String("{\"wifi\":{\"ssid\":\"") + ssid + "\",\"password\":\"" + password + "\"},"
           "\"dri\":{\"region\":\"US\",\"ua_id\":\"\",\"ua_desc\":\"\",\"op_id\":\"\","
           "\"op_secret\":\"\","
           "\"bt5_enabled\":true,\"wifi_beacon_enabled\":true,\"wifi_nan_enabled\":false}}";
}

void test_default_config_brings_the_ap_up_open() {
    config_init(&mem_storage, "DRIFT_AB");
    WifiApParams ap = wifi_ap_params();
    TEST_ASSERT_EQUAL_STRING("DRIFT_AB", ap.ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("", ap.password.c_str());
    TEST_ASSERT_FALSE(ap.secure);
}

void test_password_config_brings_the_ap_up_wpa() {
    config_init(&mem_storage, "DRIFT_AB");
    TEST_ASSERT_TRUE(config_save(config_with_ap("DroneAP", "12345678")));
    WifiApParams ap = wifi_ap_params();
    TEST_ASSERT_EQUAL_STRING("DroneAP", ap.ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("12345678", ap.password.c_str());
    TEST_ASSERT_TRUE(ap.secure);
}

void test_saved_empty_password_stays_open() {
    // The branch decision is the empty string, not a missing key: a config
    // saved with an explicit empty password must still bring the AP up open.
    config_init(&mem_storage, "DRIFT_AB");
    TEST_ASSERT_TRUE(config_save(config_with_ap("DroneAP", "")));
    WifiApParams ap = wifi_ap_params();
    TEST_ASSERT_EQUAL_STRING("", ap.password.c_str());
    TEST_ASSERT_FALSE(ap.secure);
}

void test_ap_is_pinned_to_channel_6() {
    config_init(&mem_storage, "DRIFT_AB");
    WifiApParams ap = wifi_ap_params();
    TEST_ASSERT_EQUAL_INT(6, ap.channel);
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_default_config_brings_the_ap_up_open);
    RUN_TEST(test_password_config_brings_the_ap_up_wpa);
    RUN_TEST(test_saved_empty_password_stays_open);
    RUN_TEST(test_ap_is_pinned_to_channel_6);
    return UNITY_END();
}
