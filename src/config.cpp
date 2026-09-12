#include <ArduinoJson.h>
#include <string.h>

#include "config.h"

#define KEY_WIFI_SSID "wifi_ssid"
#define KEY_WIFI_PASSWORD "wifi_password"
#define KEY_DRI_UA_ID "dri_ua_id"
#define KEY_DRI_UA_DESC "dri_ua_desc"
#define KEY_DRI_OP_ID "dri_op_id"
#define KEY_DRI_OP_SECRET "dri_op_secret"
#define KEY_DRI_REGION "dri_region"
#define KEY_BT5_ENABLED "bt5_enabled"
// NVS keys are capped at 15 characters (Preferences, nvs_partition_gen), so
// the stored key is the transport's short form; the API fields are
// wifi_beacon_enabled and wifi_nan_enabled.
#define KEY_WIFI_BEACON "wifi_beacon"
#define KEY_WIFI_NAN "wifi_nan"

static const ConfigStorage *storage;

// The regions DRIFT can be configured for. This list is the *only* thing the
// firmware knows about regions: it exists so an unknown value cannot be
// persisted and served back to the dash, not so anything can branch on it.
// Every actual region rule (which identity fields are required, the EU/UK
// registration-number format) lives in the dash - see config.h.
//
// "" is a member on purpose: it is the dash's "Other - no regional rules"
// choice, which is the right answer for the many jurisdictions that mandate
// nothing (Australia and Canada have no mandate in force) and for anyone
// whose registry DRIFT does not model. It therefore doubles as the
// never-configured value, so the firmware cannot tell a factory-fresh device
// from one where the user deliberately opted out.
static const char *const REGIONS[] = {"", "US", "EU", "UK"};

static bool region_known(const char *region) {
    for (size_t i = 0; i < sizeof(REGIONS) / sizeof(REGIONS[0]); i++)
        if (strcmp(region, REGIONS[i]) == 0)
            return true;
    return false;
}

void config_init(const ConfigStorage *storage_backend, const String &default_ssid) {
    storage = storage_backend;
    if (!storage->isKey(KEY_WIFI_SSID))
        storage->putString(KEY_WIFI_SSID, default_ssid);
    if (!storage->isKey(KEY_WIFI_PASSWORD))
        storage->putString(KEY_WIFI_PASSWORD, "");
    if (!storage->isKey(KEY_DRI_UA_ID))
        storage->putString(KEY_DRI_UA_ID, "");
    if (!storage->isKey(KEY_DRI_UA_DESC))
        storage->putString(KEY_DRI_UA_DESC, "");
    if (!storage->isKey(KEY_DRI_OP_ID))
        storage->putString(KEY_DRI_OP_ID, "");
    if (!storage->isKey(KEY_DRI_OP_SECRET))
        storage->putString(KEY_DRI_OP_SECRET, "");
    // Empty on purpose: DRIFT cannot guess which jurisdiction the aircraft
    // flies in, and guessing wrong is a compliance problem rather than an
    // inconvenience, so it claims nothing. "" is also the value the dash's
    // "Other - no regional rules" choice posts, which means an unconfigured
    // device and a deliberate opt-out are indistinguishable here.
    if (!storage->isKey(KEY_DRI_REGION))
        storage->putString(KEY_DRI_REGION, "");
    // On unless turned off: every region DRIFT targets is satisfied by
    // broadcasting BT5 alongside BT4.
    if (!storage->isKey(KEY_BT5_ENABLED))
        storage->putString(KEY_BT5_ENABLED, "1");
    // The Wi-Fi Beacon IE rides on the SoftAP beacons that go out anyway, so
    // it too defaults to on.
    if (!storage->isKey(KEY_WIFI_BEACON))
        storage->putString(KEY_WIFI_BEACON, "1");
    // Wi-Fi NAN is the one optional transport: every region is satisfied by
    // the three always-on transports, no phone ecosystem requires it, and its
    // raw-injected frames cost airtime on the SoftAP's channel - so it
    // defaults to off, like the reference implementations ship it.
    if (!storage->isKey(KEY_WIFI_NAN))
        storage->putString(KEY_WIFI_NAN, "0");
}

String config_get() {
    JsonDocument doc;
    doc["wifi"]["ssid"] = config_wifi_ssid();
    doc["wifi"]["password"] = config_wifi_password();
    doc["dri"]["region"] = config_dri_region();
    doc["dri"]["ua_id"] = config_dri_ua_id();
    doc["dri"]["ua_desc"] = config_dri_ua_desc();
    doc["dri"]["op_id"] = config_dri_op_id();
    doc["dri"]["op_secret"] = config_dri_op_secret();
    doc["dri"]["bt5_enabled"] = config_bt5_enabled();
    doc["dri"]["wifi_beacon_enabled"] = config_wifi_beacon_enabled();
    doc["dri"]["wifi_nan_enabled"] = config_wifi_nan_enabled();
    String buf;
    serializeJson(doc, buf);
    return buf;
}

// A required string field: it must be present and be a JSON string. Any
// string value is accepted, including the empty one - an empty password
// means an open AP, an empty op_id is the US case, an empty ua_desc means no
// Self-ID - so only absence and wrong types are errors. Insisting on the
// type is what stops a JSON null from being persisted as the literal string
// "null", which is what the old per-field `String x = doc[...]` reads did.
static bool read_string(const JsonDocument &doc, const char *section, const char *key,
                        const char **out) {
    JsonVariantConst value = doc[section][key];
    if (!value.is<const char *>())
        return false;
    *out = value.as<const char *>();
    return true;
}

// A required boolean field. Strict: a "true" string or a 1 is a rejection,
// not a truthy value. Under the complete-document contract this replaces the
// old "absent or non-boolean keeps the default" rule - that rule existed to
// stop a partial POST from silently disabling a broadcast, and a partial
// POST is now rejected outright instead.
static bool read_bool(const JsonDocument &doc, const char *section, const char *key, bool *out) {
    JsonVariantConst value = doc[section][key];
    if (!value.is<bool>())
        return false;
    *out = value.as<bool>();
    return true;
}

bool config_save(String data) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, data);
    if (err) {
        Serial.println("Config rejected: malformed JSON");
        return false;
    }

    // Validate the entire document before writing any part of it, so a
    // rejected POST cannot leave storage half-updated.
    const char *wifi_ssid;
    const char *wifi_password;
    const char *dri_ua_id;
    const char *dri_ua_desc;
    const char *dri_op_id;
    const char *dri_op_secret;
    const char *dri_region;
    bool bt5_enabled;
    bool wifi_beacon_enabled;
    bool wifi_nan_enabled;
    if (!read_string(doc, "wifi", "ssid", &wifi_ssid) ||
        !read_string(doc, "wifi", "password", &wifi_password) ||
        !read_string(doc, "dri", "region", &dri_region) ||
        !read_string(doc, "dri", "ua_id", &dri_ua_id) ||
        !read_string(doc, "dri", "ua_desc", &dri_ua_desc) ||
        !read_string(doc, "dri", "op_id", &dri_op_id) ||
        !read_string(doc, "dri", "op_secret", &dri_op_secret) ||
        !read_bool(doc, "dri", "bt5_enabled", &bt5_enabled) ||
        !read_bool(doc, "dri", "wifi_beacon_enabled", &wifi_beacon_enabled) ||
        !read_bool(doc, "dri", "wifi_nan_enabled", &wifi_nan_enabled)) {
        Serial.println("Config rejected: incomplete or wrong-typed document");
        return false;
    }
    if (!region_known(dri_region)) {
        Serial.println("Config rejected: unknown region");
        return false;
    }

    storage->putString(KEY_WIFI_SSID, wifi_ssid);
    storage->putString(KEY_WIFI_PASSWORD, wifi_password);
    storage->putString(KEY_DRI_REGION, dri_region);
    storage->putString(KEY_DRI_UA_ID, dri_ua_id);
    storage->putString(KEY_DRI_UA_DESC, dri_ua_desc);
    storage->putString(KEY_DRI_OP_ID, dri_op_id);
    storage->putString(KEY_DRI_OP_SECRET, dri_op_secret);
    // Booleans ride the string-only storage seam as "1"/"0".
    storage->putString(KEY_BT5_ENABLED, bt5_enabled ? "1" : "0");
    storage->putString(KEY_WIFI_BEACON, wifi_beacon_enabled ? "1" : "0");
    storage->putString(KEY_WIFI_NAN, wifi_nan_enabled ? "1" : "0");
    return true;
}

String config_wifi_ssid() {
    return storage->getString(KEY_WIFI_SSID);
}

String config_wifi_password() {
    return storage->getString(KEY_WIFI_PASSWORD);
}

String config_dri_ua_id() {
    return storage->getString(KEY_DRI_UA_ID);
}

String config_dri_ua_desc() {
    return storage->getString(KEY_DRI_UA_DESC);
}

String config_dri_op_id() {
    return storage->getString(KEY_DRI_OP_ID);
}

// Deliberately not passed to dri_populate_identity(): the verification code
// is never broadcast. It is stored only so the dash can reassemble the full
// registration string for display and re-run the checksum check.
String config_dri_op_secret() {
    return storage->getString(KEY_DRI_OP_SECRET);
}

String config_dri_region() {
    return storage->getString(KEY_DRI_REGION);
}

// Booleans ride the string-only storage seam as "1"/"0" (the ESP backend
// persists strings; the e2e NVS seed writes the same encoding).
bool config_bt5_enabled() {
    return storage->getString(KEY_BT5_ENABLED) != "0";
}

bool config_wifi_beacon_enabled() {
    return storage->getString(KEY_WIFI_BEACON) != "0";
}

// Opt-in transport: only an explicit "1" turns it on.
bool config_wifi_nan_enabled() {
    return storage->getString(KEY_WIFI_NAN) == "1";
}
