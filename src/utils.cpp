#include <Arduino.h>

#include "utils.h"

const char *getDefaultSSID() {
    uint64_t mac = ESP.getEfuseMac();
    char *formatted = (char *)malloc(DEFAULT_SSID_LENGTH);
    snprintf(formatted, DEFAULT_SSID_LENGTH, DEFAULT_SSID_PREFIX, (mac >> 32) & 0xFF, (mac >> 40) & 0xFF);
    return formatted;
}