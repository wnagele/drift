#include <HardwareSerial.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncHTTPUpdateServer.h>
#include "betaflight_mavlink.h"
#include "dri.h"
#include "dash.h"
#include "config.h"

ESPAsyncHTTPUpdateServer updateServer;
AsyncWebServer server(80);

mavlink_state_t mavlink_state;
ODID_UAS_Data odid_state;

bool armed = false;
bool gps_fix = false;

void setup() {
    Serial.begin(9600);

    config_init();

    String wifi_ssid = config_wifi_ssid();
    String wifi_password = config_wifi_password();
    if (wifi_password != "") {
        WiFi.softAP(wifi_ssid, wifi_password);
    } else {
        WiFi.softAP(wifi_ssid);
    }

    updateServer.setup(&server);
    server.begin();
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse(200, "text/html", DASH, sizeof(DASH));
        response->addHeader("Content-Encoding", "gzip");
        request->send(response);
    });
    mavlink_init(&mavlink_state);
    dri_init(&odid_state);
}

void loop() {
    switch (mavlink_parse(&mavlink_state)) {
        case HEARTBEAT:
            switch (mavlink_state.heartbeat.system_status) {
                case MAV_STATE_STANDBY:
                    dri_update_status(&odid_state, ODID_STATUS_GROUND);
                    armed = false;
                    break;
                case MAV_STATE_ACTIVE:
                    dri_update_status(&odid_state, ODID_STATUS_AIRBORNE);
                    armed = true;
                    break;
                case MAV_STATE_CRITICAL:
                case MAV_STATE_EMERGENCY:
                case MAV_STATE_FLIGHT_TERMINATION:
                    dri_update_status(&odid_state, ODID_STATUS_EMERGENCY);
                    armed = true;
                    break;
                default:
                    dri_update_status(&odid_state, ODID_STATUS_UNDECLARED);
                    armed = false;
                    break;
            }
            break;
        case GPS_RAW_INT:
            gps_fix = mavlink_state.gps_raw_int.fix_type >= GPS_FIX_TYPE_3D_FIX;
            break;
        case GLOBAL_POSITION_INT: {
            if (!gps_fix)
                break;
            float relative_alt = INV_ALT;
            float hdg = INV_DIR;
            if (armed) {
                relative_alt = mavlink_state.global_position_int.relative_alt / (float)1000;
                hdg = mavlink_state.global_position_int.hdg;
            }
            dri_update_location(
                &odid_state,
                mavlink_state.global_position_int.lat / (double)10000000,
                mavlink_state.global_position_int.lon / (double)10000000,
                mavlink_state.global_position_int.alt / (float)1000,
                relative_alt,
                hdg
            );
            break;
        }
        case GPS_GLOBAL_ORIGIN:
            if (!gps_fix)
                break;
            dri_update_operator(
                &odid_state,
                mavlink_state.gps_global_origin.latitude / (double)10000000,
                mavlink_state.gps_global_origin.longitude / (double)10000000,
                mavlink_state.gps_global_origin.altitude / (float)1000
            );
            break;
    }
    dri_transmit(&odid_state);
}