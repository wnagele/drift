#ifndef DRIFT_WIFI_AP_H
#define DRIFT_WIFI_AP_H

#include <Arduino.h>

// Host-testable seam for the softAP bring-up in net_init(). Whether the AP is
// open or WPA-protected is a config decision (empty password = open network);
// it is asserted here instead of only inside the ESP32-only WiFi calls.

struct WifiApParams {
    String ssid;
    String password;  // empty: open network
    bool secure;      // true: start the AP with WPA2
};

WifiApParams wifi_ap_params();

#endif
