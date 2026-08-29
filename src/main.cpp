#include <HardwareSerial.h>
#include <TaskScheduler.h>
#include "betaflight_mavlink.h"
#include "ble.h"
#include "config.h"
#include "dri.h"
#include "net.h"
#include "status.h"
#include "txcount.h"
#include "utils.h"
#include "wifi_beacon.h"
#include "wifi_nan.h"

Scheduler scheduler;

mavlink_state_t mavlink_state;
ODID_UAS_Data odid_state;

bool armed = false;
bool gps_fix = false;

void processStatus() {
    status_process();
}
Task taskProcessStatus(3*TASK_SECOND, TASK_FOREVER, &processStatus, &scheduler, true);

void sendStatus() {
    net_broadcast(status_get());
}
Task taskSendStatus(1*TASK_SECOND, TASK_FOREVER, &sendStatus, &scheduler, true);

void setup() {
    Serial.begin(9600);

    config_init(config_storage_esp(), getDefaultSSID());

    net_init();

    ble_init(config_wifi_ssid().c_str());
    ble5_init(config_bt5_enabled());
    wifi_beacon_init(config_wifi_beacon_enabled());

    // The Wi-Fi NAN frames embed the eFuse base MAC (readable before the
    // WiFi stack starts, unlike the SoftAP's own address).
    uint8_t src_mac[6];
    getBaseMac(src_mac);
    wifi_nan_init(config_wifi_nan_enabled(), src_mac);

    // The transmit-rate diagnostics gate on the same config flags as the
    // radio seams, so a disabled transport cannot report phantom activity.
    txcount_init(config_bt5_enabled(), config_wifi_beacon_enabled(),
                 config_wifi_nan_enabled());

    mavlink_init(&mavlink_state);

    dri_init(&odid_state, millis());

    dri_populate_identity(
        &odid_state,
        config_dri_ua_id().c_str(),
        config_dri_op_id().c_str(),
        config_dri_ua_desc().c_str()
    );

    Serial.println("DRIFT boot"); // boot marker
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
            status_telemetry_rcvd();
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
                if (relative_alt < MIN_ALT || relative_alt > MAX_ALT)
                    relative_alt = INV_ALT;   // out of ODID range: report "unknown"
                hdg = mavlink_state.global_position_int.hdg / (float)100;
                if (hdg > MAX_DIR)
                    hdg = INV_DIR;            // incl. MAVLink's UINT16_MAX "heading unknown"
            }
            float alt = mavlink_state.global_position_int.alt / (float)1000;
            if (alt < MIN_ALT || alt > MAX_ALT)
                alt = INV_ALT;                // out of ODID range: report "unknown"
            dri_update_location(
                &odid_state,
                mavlink_state.global_position_int.lat / (double)10000000,
                mavlink_state.global_position_int.lon / (double)10000000,
                alt,
                relative_alt,
                hdg
            );
            status_gnss_rcvd();
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

    dri_transmit(&odid_state, millis());
    txcount_sample(millis());

    scheduler.execute();
}
