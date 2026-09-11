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
            float direction = INV_DIR;
            float speed_horizontal = INV_SPEED_H;
            float speed_vertical = INV_SPEED_V;
            if (armed) {
                relative_alt = mavlink_state.global_position_int.relative_alt / (float)1000;
                if (relative_alt < MIN_ALT || relative_alt > MAX_ALT)
                    relative_alt = INV_ALT;   // out of ODID range: report "unknown"
                // ASTM F3411's Direction is the route course over ground, not
                // the airframe's yaw: GPS_RAW_INT.cog, not
                // GLOBAL_POSITION_INT.hdg. A hovering or sideways-drifting
                // aircraft points somewhere other than where it travels, and
                // a receiver plotting yaw as track draws the wrong path.
                direction = mavlink_state.gps_raw_int.cog / (float)100;
                if (direction > MAX_DIR)
                    direction = INV_DIR;      // incl. MAVLink's UINT16_MAX "course unknown"
                // Ground speed. Out-of-range speeds are clamped, not
                // downgraded to "unknown": ODID's rule is to report the
                // ceiling for a faster aircraft, and the encoder rejects
                // anything between MAX_SPEED_H and the INV_SPEED_H
                // sentinel outright. MAVLink's UINT16_MAX really does mean
                // "unknown", so it is checked before the clamp - otherwise
                // a missing speed would be broadcast as 254.25 m/s.
                if (mavlink_state.gps_raw_int.vel != UINT16_MAX) {
                    speed_horizontal = mavlink_state.gps_raw_int.vel / (float)100;
                    if (speed_horizontal > MAX_SPEED_H)
                        speed_horizontal = MAX_SPEED_H;
                }
                // Vertical speed. MAVLink's NED vz is positive *down*,
                // ODID's SpeedVertical is positive *up* (per the identical
                // field in MAVLink's own OPEN_DRONE_ID_LOCATION), so the
                // sign flips. vz has no "unknown" sentinel, so the value is
                // always taken and only clamped to the ODID range.
                speed_vertical = -(mavlink_state.global_position_int.vz / (float)100);
                if (speed_vertical > MAX_SPEED_V)
                    speed_vertical = MAX_SPEED_V;
                else if (speed_vertical < MIN_SPEED_V)
                    speed_vertical = MIN_SPEED_V;
            }
            float alt = mavlink_state.global_position_int.alt / (float)1000;
            if (alt < MIN_ALT || alt > MAX_ALT)
                alt = INV_ALT;                // out of ODID range: report "unknown"
            // The UTC mark of the position source output. Unlike height,
            // course and speed this is not arm-gated: it timestamps the
            // position, which is broadcast on the ground too. The flight
            // controller's own UTC clock is the only source - DRIFT has no
            // RTC - so it stays "unknown" until a SYSTEM_TIME arrives.
            float timestamp = dri_location_timestamp(
                mavlink_state.system_time.time_unix_usec);
            // Zero-initialised so a field added to DriLocation later cannot
            // reach the encoder as stack garbage; every ODID "unknown"
            // sentinel that matters here is 0 anyway.
            DriLocation location = {};
            location.latitude = mavlink_state.global_position_int.lat / (double)10000000;
            location.longitude = mavlink_state.global_position_int.lon / (double)10000000;
            location.altitude_geo = alt;
            location.height = relative_alt;
            location.direction = direction;
            location.speed_horizontal = speed_horizontal;
            location.speed_vertical = speed_vertical;
            location.timestamp = timestamp;
            // GNSS uncertainty, straight from the receiver's own estimate.
            // These are MAVLink v2 extension fields, so a v1 sender (which is
            // what Betaflight speaks) truncates them away and they arrive as
            // 0 - reported as "unknown" rather than invented from HDOP/VDOP,
            // which would need a guessed UERE constant and would dress an
            // assumption up as a measurement in a field a receiver may trust.
            location.horizontal_accuracy =
                dri_horizontal_accuracy(mavlink_state.gps_raw_int.h_acc);
            location.vertical_accuracy =
                dri_vertical_accuracy(mavlink_state.gps_raw_int.v_acc);
            location.speed_accuracy =
                dri_speed_accuracy(mavlink_state.gps_raw_int.vel_acc);
            dri_update_location(&odid_state, &location);
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
