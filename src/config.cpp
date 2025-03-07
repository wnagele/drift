#include <Preferences.h>
#include "utils.h"

#include "config.h"

#define PREFS_NAMESPACE "drift"
#define PREFS_WIFI_SSID "wifi_ssid"
#define PREFS_WIFI_PASSWORD "wifi_password"

Preferences prefs;

void config_init() {
    prefs.begin(PREFS_NAMESPACE, false);
    if (!prefs.isKey(PREFS_WIFI_SSID))
        prefs.putString(PREFS_WIFI_SSID, getDefaultSSID());
    if (!prefs.isKey(PREFS_WIFI_PASSWORD))
        prefs.putString(PREFS_WIFI_PASSWORD, "");
}

String config_wifi_ssid() {
    return prefs.getString(PREFS_WIFI_SSID);
}

String config_wifi_password() {
    return prefs.getString(PREFS_WIFI_PASSWORD);
}