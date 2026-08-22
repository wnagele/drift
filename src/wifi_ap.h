#ifndef DRIFT_WIFI_AP_H
#define DRIFT_WIFI_AP_H

#include <Arduino.h>

// Host-testable seam for the softAP bring-up in net_init(). Whether the AP is
// open or WPA-protected is a config decision (empty password = open network);
// it is asserted here instead of only inside the ESP32-only WiFi calls.

// ODID Wi-Fi broadcasts (Beacon vendor IE / NAN) only reach receivers on 
// channel 6; the AP is pinned there.
#define WIFI_AP_CHANNEL 6

struct WifiApParams {
    String ssid;
    String password;  // empty: open network
    bool secure;      // true: start the AP with WPA2
    int channel;      // fixed: WIFI_AP_CHANNEL
};

WifiApParams wifi_ap_params();

#endif
