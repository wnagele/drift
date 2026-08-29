#include <Arduino.h>

#include "config_storage.h"

void config_init(const ConfigStorage *storage, const String &default_ssid);
String config_get();
void config_save(String data);
String config_wifi_ssid();
String config_wifi_password();
String config_dri_ua_id();
String config_dri_ua_desc();
String config_dri_op_id();
bool config_bt5_enabled();
bool config_wifi_beacon_enabled();
