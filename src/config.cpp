#include <ArduinoJson.h>

#include "config.h"

#define KEY_WIFI_SSID "wifi_ssid"
#define KEY_WIFI_PASSWORD "wifi_password"
#define KEY_DRI_UA_ID "dri_ua_id"
#define KEY_DRI_UA_DESC "dri_ua_desc"
#define KEY_DRI_OP_ID "dri_op_id"
#define KEY_BT5_ENABLED "bt5_enabled"

static const ConfigStorage *storage;

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
    // On unless turned off: both region profiles DRIFT targets broadcast
    // BT5 alongside BT4.
    if (!storage->isKey(KEY_BT5_ENABLED))
        storage->putString(KEY_BT5_ENABLED, "1");
}

String config_get() {
    JsonDocument doc;
    doc["wifi"]["ssid"] = config_wifi_ssid();
    doc["wifi"]["password"] = config_wifi_password();
    doc["dri"]["ua_id"] = config_dri_ua_id();
    doc["dri"]["ua_desc"] = config_dri_ua_desc();
    doc["dri"]["op_id"] = config_dri_op_id();
    doc["dri"]["bt5_enabled"] = config_bt5_enabled();
    String buf;
    serializeJson(doc, buf);
    return buf;
}

void config_save(String data) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, data);
    if (err) {
        Serial.println("JSON Error");
        return;
    }
    String wifi_ssid = doc["wifi"]["ssid"];
    storage->putString(KEY_WIFI_SSID, wifi_ssid);
    String wifi_password = doc["wifi"]["password"];
    storage->putString(KEY_WIFI_PASSWORD, wifi_password);
    String dri_ua_id = doc["dri"]["ua_id"];
    storage->putString(KEY_DRI_UA_ID, dri_ua_id);
    String dri_ua_desc = doc["dri"]["ua_desc"];
    storage->putString(KEY_DRI_UA_DESC, dri_ua_desc);
    String dri_op_id = doc["dri"]["op_id"];
    storage->putString(KEY_DRI_OP_ID, dri_op_id);
    // A missing field must not silently disable the broadcast: absent or
    // non-boolean values keep the default (on).
    bool bt5_enabled = doc["dri"]["bt5_enabled"] | true;
    storage->putString(KEY_BT5_ENABLED, bt5_enabled ? "1" : "0");
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

// Booleans ride the string-only storage seam as "1"/"0" (the ESP backend
// persists strings; the e2e NVS seed writes the same encoding).
bool config_bt5_enabled() {
    return storage->getString(KEY_BT5_ENABLED) != "0";
}
