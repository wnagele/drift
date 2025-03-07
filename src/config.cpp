#include <Preferences.h>
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

String config_wifi_ssid() {
    return prefs.getString(PREFS_WIFI_SSID);
}

String config_wifi_password() {
    return prefs.getString(PREFS_WIFI_PASSWORD);
}

String dri_ua_id() {
    return prefs.getString(PREFS_DRI_UA_ID);
}

String dri_ua_desc() {
    return prefs.getString(PREFS_DRI_UA_DESC);
}

String dri_op_id() {
    return prefs.getString(PREFS_DRI_OP_ID);
}