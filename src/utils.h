#include <Arduino.h>
#include <stdint.h>

#define DEFAULT_SSID_FORMAT "DRIFT_%02X%02X"
#define DEFAULT_SSID_LENGTH 11 // DRIFT_B33F\0

String formatDefaultSSID(uint64_t mac);
String getDefaultSSID();
