#include <Arduino.h>
#include <string.h>

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
// getEfuseMac() packs the MAC little-endian (esp_efuse_mac_get_default() into
// a uint64_t), so the LSB is the first octet. The same base MAC feeds
// getDefaultSSID(); unlike the SoftAP's own base+1 address it is readable
// before the WiFi stack starts, which the QEMU e2e build relies on.
void getBaseMac(uint8_t mac[6]) {
    uint64_t base = ESP.getEfuseMac();
    for (int i = 0; i < 6; i++)
        mac[i] = (uint8_t)(base >> (8 * i));
}
#else // no eFuse MAC (native tests)
String getDefaultSSID() {
    return formatDefaultSSID(0);
}
void getBaseMac(uint8_t mac[6]) {
    memset(mac, 0, 6);
}
#endif
