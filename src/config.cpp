#include <Preferences.h>
#include <ArduinoJson.h>
#include "utils.h"

#include "config.h"

#define PREFS_NAMESPACE "drift"
#define PREFS_WIFI_SSID "wifi_ssid"
#define PREFS_WIFI_PASSWORD "wifi_password"
#define PREFS_DRI_UA_ID "dri_ua_id"
#define PREFS_DRI_UA_DESC "dri_ua_desc"
#define PREFS_DRI_OP_ID "dri_op_id"

Preferences prefs;

void config_init() {
    prefs.begin(PREFS_NAMESPACE, false);
    if (!prefs.isKey(PREFS_WIFI_SSID))
        prefs.putString(PREFS_WIFI_SSID, getDefaultSSID());
    if (!prefs.isKey(PREFS_WIFI_PASSWORD))
        prefs.putString(PREFS_WIFI_PASSWORD, "");
    if (!prefs.isKey(PREFS_DRI_UA_ID))
        prefs.putString(PREFS_DRI_UA_ID, "");
    if (!prefs.isKey(PREFS_DRI_UA_DESC))
        prefs.putString(PREFS_DRI_UA_DESC, "");
    if (!prefs.isKey(PREFS_DRI_OP_ID))
        prefs.putString(PREFS_DRI_OP_ID, "");
}

String config_get() {
    JsonDocument doc;
    doc["wifi"]["ssid"] = config_wifi_ssid();
    doc["wifi"]["password"] = config_wifi_password();
    doc["dri"]["ua_id"] = config_dri_ua_id();
    doc["dri"]["ua_desc"] = config_dri_ua_desc();
    doc["dri"]["op_id"] = config_dri_op_id();
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
    prefs.putString(PREFS_WIFI_SSID, wifi_ssid);
    String wifi_password = doc["wifi"]["password"];
    prefs.putString(PREFS_WIFI_PASSWORD, wifi_password);
    String dri_ua_id = doc["dri"]["ua_id"];
    prefs.putString(PREFS_DRI_UA_ID, dri_ua_id);
    String dri_ua_desc = doc["dri"]["ua_desc"];
    prefs.putString(PREFS_DRI_UA_DESC, dri_ua_desc);
    String dri_op_id = doc["dri"]["op_id"];
    prefs.putString(PREFS_DRI_OP_ID, dri_op_id);
}

String config_wifi_ssid() {
    return prefs.getString(PREFS_WIFI_SSID);
}

String config_wifi_password() {
    return prefs.getString(PREFS_WIFI_PASSWORD);
}

String config_dri_ua_id() {
    return prefs.getString(PREFS_DRI_UA_ID);
}

String config_dri_ua_desc() {
    return prefs.getString(PREFS_DRI_UA_DESC);
}

String config_dri_op_id() {
    return prefs.getString(PREFS_DRI_OP_ID);
}