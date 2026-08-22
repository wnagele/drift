#include "config.h"
#include "wifi_ap.h"

WifiApParams wifi_ap_params() {
    WifiApParams params;
    params.ssid = config_wifi_ssid();
    params.password = config_wifi_password();
    params.secure = params.password != "";
    return params;
}
