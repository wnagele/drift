#include <Arduino.h>
#include <Preferences.h>

#include "config_storage.h"

#define PREFS_NAMESPACE "drift"

static Preferences prefs;

static bool esp_isKey(const char *key) {
    return prefs.isKey(key);
}

static String esp_getString(const char *key) {
    return prefs.getString(key);
}

static void esp_putString(const char *key, const String &value) {
    prefs.putString(key, value);
}

static const ConfigStorage storage = { esp_isKey, esp_getString, esp_putString };

const ConfigStorage *config_storage_esp() {
    if (!prefs.begin(PREFS_NAMESPACE, false)) {
        Serial.println("[E][ConfigStorage] prefs.begin() failed");
    }
    return &storage;
}
