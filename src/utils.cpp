#include <Arduino.h>

#include "utils.h"

String formatDefaultSSID(uint64_t mac) {
    char formatted[DEFAULT_SSID_LENGTH];
    unsigned lower = (unsigned)((mac >> 32) & 0xFF);
    unsigned upper = (unsigned)((mac >> 40) & 0xFF);
    snprintf(formatted, DEFAULT_SSID_LENGTH, DEFAULT_SSID_FORMAT, lower, upper);
    return String(formatted);
}

#if defined(ESP32)
String getDefaultSSID() {
    return formatDefaultSSID(ESP.getEfuseMac());
}
#else // no eFuse MAC (native tests)
String getDefaultSSID() {
    return formatDefaultSSID(0);
}
#endif
