#ifndef DRIFT_CONFIG_STORAGE_H
#define DRIFT_CONFIG_STORAGE_H

#include <Arduino.h>

// Storage seam for configuration persistence. The firmware uses the
// Preferences-backed implementation (config_storage_esp.cpp); tests can
// substitute any other backend (e.g. in-memory) via config_init().
struct ConfigStorage {
    bool (*isKey)(const char *key);
    String (*getString)(const char *key);
    void (*putString)(const char *key, const String &value);
};

// Preferences-backed storage (target builds only; defined in
// config_storage_esp.cpp). Opens the "drift" namespace.
const ConfigStorage *config_storage_esp();

#endif
