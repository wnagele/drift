#include <Arduino.h>

#include "utils.h"

String getDefaultSSID() {
    uint64_t mac = ESP.getEfuseMac();
    char formatted[DEFAULT_SSID_LENGTH];
    snprintf(formatted, DEFAULT_SSID_LENGTH, DEFAULT_SSID_FORMAT, (mac >> 32) & 0xFF, (mac >> 40) & 0xFF);
    return String(formatted);
}