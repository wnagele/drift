// Generates the MAVLink test captures shared by the native unit tests and the
// e2e suite: one raw stream per scenario in test/fixtures/mavlink/<name>.bin.
// The fixtures only define the inputs - the expected decoded values are owned
// by the test cases themselves. Regenerate via `make fixtures` (see Makefile).
//
// Frames are emitted as MAVLink v1 (like Betaflight's MAVLink telemetry).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <MAVLink.h>

#define SYS_ID 1
#define COMP_ID 1

// Stream disarmed_fix: 3D fix, disarmed.
#define A_GPS_FIX_TYPE 3
#define A_GPS_LAT 473566000
#define A_GPS_LON 85321000
#define A_GPS_ALT 500000
#define A_GPI_LAT 473566123
#define A_GPI_LON 85321456
#define A_GPI_ALT 500500
#define A_GPI_REL_ALT 30500
#define A_GPI_HDG 9000
#define A_HB_STATUS MAV_STATE_STANDBY
#define A_HB_BASE_MODE 0

// Stream armed_no_fix: no fix, armed.
#define B_GPS_FIX_TYPE 1
#define B_HB_STATUS MAV_STATE_ACTIVE
#define B_HB_BASE_MODE MAV_MODE_FLAG_SAFETY_ARMED

// Stream origin_set: 3D fix, origin set.
#define C_ORIGIN_LAT 473566000
#define C_ORIGIN_LON 85321000
#define C_ORIGIN_ALT 500000

// Stream armed_unknown: 3D fix, armed, heading unknown and altitudes beyond
// any ODID representation. MAVLink defines hdg = UINT16_MAX as "heading
// unknown"; relative_alt/alt near INT32_MAX (mm) are far outside the ODID
// altitude range. main.cpp must map all of these to the ODID "unknown"
// sentinels - not feed 655.35 deg / 2147483 m to the encoder.
#define D_GPS_FIX_TYPE 3
#define D_GPI_LAT 473566123
#define D_GPI_LON 85321456
#define D_GPI_ALT 2147483647
#define D_GPI_REL_ALT 2147483647
#define D_GPI_HDG UINT16_MAX
#define D_HB_STATUS MAV_STATE_ACTIVE
#define D_HB_BASE_MODE MAV_MODE_FLAG_SAFETY_ARMED

// Stream no_fix_position: no fix, armed; a position and an origin arrive
// anyway. main.cpp must gate both on gps_fix. Coordinates deliberately
// differ from every other stream, so a broken gate visibly changes state.
#define E_GPS_FIX_TYPE 1
#define E_GPI_LAT 470000000
#define E_GPI_LON 80000000
#define E_GPI_ALT 123000
#define E_GPI_REL_ALT 45000
#define E_GPI_HDG 1200
#define E_ORIGIN_LAT 470000000
#define E_ORIGIN_LON 80000000
#define E_ORIGIN_ALT 123000
#define E_HB_STATUS MAV_STATE_ACTIVE
#define E_HB_BASE_MODE MAV_MODE_FLAG_SAFETY_ARMED

// Stream fix_2d: 2D fix, armed; the position must still be ignored - only a
// 3D fix counts as a fix.
#define F_GPS_FIX_TYPE 2

// Streams emergency / undeclared: heartbeats with failsafe statuses
// (critical, flight termination -> ODID_STATUS_EMERGENCY) and an unmapped
// status (poweroff -> ODID_STATUS_UNDECLARED, disarmed).

static uint8_t stream[1024];
static size_t stream_len;

static void reset_stream() {
    stream_len = 0;
    // Each capture is independent: restart the MAVLink packet sequence so a
    // stream's bytes depend only on its own messages, not on the emission
    // order in this generator.
    mavlink_get_channel_status(MAVLINK_COMM_0)->current_tx_seq = 0;
}

static void append_msg(mavlink_message_t *msg) {
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    uint16_t len = mavlink_msg_to_send_buffer(buf, msg);
    memcpy(stream + stream_len, buf, len);
    stream_len += len;
}

static FILE *open_fixture(const char *name, const char *mode) {
    char path[256];
    snprintf(path, sizeof path, "test/fixtures/mavlink/%s", name);
    FILE *f = fopen(path, mode);
    if (!f) {
        fprintf(stderr, "cannot open %s (run from repo root)\n", path);
        exit(1);
    }
    return f;
}

static void emit_stream(const char *name) {
    FILE *f = open_fixture(name, "wb");
    fwrite(stream, 1, stream_len, f);
    fclose(f);
    printf("wrote test/fixtures/mavlink/%s (%zu bytes)\n", name, (size_t)stream_len);
}

int main() {
    // Emit MAVLink v1 frames like Betaflight.
    mavlink_status_t *chan = mavlink_get_channel_status(MAVLINK_COMM_0);
    chan->flags |= MAVLINK_STATUS_FLAG_OUT_MAVLINK1;

    mavlink_message_t msg;

    // --- disarmed_fix: GPS_RAW_INT (3D fix) + GLOBAL_POSITION_INT + HEARTBEAT (standby)
    reset_stream();
    mavlink_msg_gps_raw_int_pack(SYS_ID, COMP_ID, &msg,
        1234567000, A_GPS_FIX_TYPE, A_GPS_LAT, A_GPS_LON, A_GPS_ALT,
        150, 250, 350, 9000, 10,
        A_GPS_ALT, 120, 200, 80, 500, 0);
    append_msg(&msg);
    mavlink_msg_global_position_int_pack(SYS_ID, COMP_ID, &msg,
        1000, A_GPI_LAT, A_GPI_LON, A_GPI_ALT, A_GPI_REL_ALT, 120, -45, -30, A_GPI_HDG);
    append_msg(&msg);
    mavlink_msg_heartbeat_pack(SYS_ID, COMP_ID, &msg,
        MAV_TYPE_QUADROTOR, MAV_AUTOPILOT_GENERIC, A_HB_BASE_MODE, 0, A_HB_STATUS);
    append_msg(&msg);
    emit_stream("disarmed_fix.bin");

    // --- armed_no_fix: GPS_RAW_INT (no fix) + HEARTBEAT (active, armed)
    reset_stream();
    mavlink_msg_gps_raw_int_pack(SYS_ID, COMP_ID, &msg,
        1234567000, B_GPS_FIX_TYPE, 0, 0, 0,
        UINT16_MAX, UINT16_MAX, 0, 0, 4,
        0, UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX, 0);
    append_msg(&msg);
    mavlink_msg_heartbeat_pack(SYS_ID, COMP_ID, &msg,
        MAV_TYPE_QUADROTOR, MAV_AUTOPILOT_GENERIC, B_HB_BASE_MODE, 0, B_HB_STATUS);
    append_msg(&msg);
    emit_stream("armed_no_fix.bin");

    // --- armed_fix: GPS_RAW_INT (3D fix) + HEARTBEAT (active, armed) + GLOBAL_POSITION_INT
    // The primary real-world scenario: an armed aircraft with good telemetry,
    // so height and direction are broadcast alongside the position.
    reset_stream();
    mavlink_msg_gps_raw_int_pack(SYS_ID, COMP_ID, &msg,
        1234567000, A_GPS_FIX_TYPE, A_GPS_LAT, A_GPS_LON, A_GPS_ALT,
        150, 250, 350, 9000, 10,
        A_GPS_ALT, 120, 200, 80, 500, 0);
    append_msg(&msg);
    mavlink_msg_heartbeat_pack(SYS_ID, COMP_ID, &msg,
        MAV_TYPE_QUADROTOR, MAV_AUTOPILOT_GENERIC, B_HB_BASE_MODE, 0, B_HB_STATUS);
    append_msg(&msg);
    mavlink_msg_global_position_int_pack(SYS_ID, COMP_ID, &msg,
        1000, A_GPI_LAT, A_GPI_LON, A_GPI_ALT, A_GPI_REL_ALT, 120, -45, -30, A_GPI_HDG);
    append_msg(&msg);
    emit_stream("armed_fix.bin");

    // --- origin_set: GPS_RAW_INT (3D fix) + GPS_GLOBAL_ORIGIN
    reset_stream();
    mavlink_msg_gps_raw_int_pack(SYS_ID, COMP_ID, &msg,
        1234567000, A_GPS_FIX_TYPE, A_GPS_LAT, A_GPS_LON, A_GPS_ALT,
        150, 250, 350, 9000, 10,
        A_GPS_ALT, 120, 200, 80, 500, 0);
    append_msg(&msg);
    mavlink_msg_gps_global_origin_pack(SYS_ID, COMP_ID, &msg,
        C_ORIGIN_LAT, C_ORIGIN_LON, C_ORIGIN_ALT, 1234567000);
    append_msg(&msg);
    emit_stream("origin_set.bin");

    // --- armed_unknown: GPS_RAW_INT (3D fix) + HEARTBEAT (active, armed) +
    // GLOBAL_POSITION_INT (hdg = UINT16_MAX "unknown", extreme altitudes)
    reset_stream();
    mavlink_msg_gps_raw_int_pack(SYS_ID, COMP_ID, &msg,
        1234567000, D_GPS_FIX_TYPE, A_GPS_LAT, A_GPS_LON, A_GPS_ALT,
        150, 250, 350, 9000, 10,
        A_GPS_ALT, 120, 200, 80, 500, 0);
    append_msg(&msg);
    mavlink_msg_heartbeat_pack(SYS_ID, COMP_ID, &msg,
        MAV_TYPE_QUADROTOR, MAV_AUTOPILOT_GENERIC, D_HB_BASE_MODE, 0, D_HB_STATUS);
    append_msg(&msg);
    mavlink_msg_global_position_int_pack(SYS_ID, COMP_ID, &msg,
        1000, D_GPI_LAT, D_GPI_LON, D_GPI_ALT, D_GPI_REL_ALT, 120, -45, -30, D_GPI_HDG);
    append_msg(&msg);
    emit_stream("armed_unknown.bin");

    // --- no_fix_position: GPS_RAW_INT (no fix) + GLOBAL_POSITION_INT +
    // GPS_GLOBAL_ORIGIN + HEARTBEAT (active, armed; last, for sync)
    reset_stream();
    mavlink_msg_gps_raw_int_pack(SYS_ID, COMP_ID, &msg,
        1234567000, E_GPS_FIX_TYPE, 0, 0, 0,
        UINT16_MAX, UINT16_MAX, 0, 0, 4,
        0, UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX, 0);
    append_msg(&msg);
    mavlink_msg_global_position_int_pack(SYS_ID, COMP_ID, &msg,
        1000, E_GPI_LAT, E_GPI_LON, E_GPI_ALT, E_GPI_REL_ALT, 120, -45, -30, E_GPI_HDG);
    append_msg(&msg);
    mavlink_msg_gps_global_origin_pack(SYS_ID, COMP_ID, &msg,
        E_ORIGIN_LAT, E_ORIGIN_LON, E_ORIGIN_ALT, 1234567000);
    append_msg(&msg);
    mavlink_msg_heartbeat_pack(SYS_ID, COMP_ID, &msg,
        MAV_TYPE_QUADROTOR, MAV_AUTOPILOT_GENERIC, E_HB_BASE_MODE, 0, E_HB_STATUS);
    append_msg(&msg);
    emit_stream("no_fix_position.bin");

    // --- fix_2d: GPS_RAW_INT (2D fix) + GLOBAL_POSITION_INT + HEARTBEAT
    reset_stream();
    mavlink_msg_gps_raw_int_pack(SYS_ID, COMP_ID, &msg,
        1234567000, F_GPS_FIX_TYPE, 0, 0, 0,
        UINT16_MAX, UINT16_MAX, 0, 0, 4,
        0, UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX, 0);
    append_msg(&msg);
    mavlink_msg_global_position_int_pack(SYS_ID, COMP_ID, &msg,
        1000, E_GPI_LAT, E_GPI_LON, E_GPI_ALT, E_GPI_REL_ALT, 120, -45, -30, E_GPI_HDG);
    append_msg(&msg);
    mavlink_msg_heartbeat_pack(SYS_ID, COMP_ID, &msg,
        MAV_TYPE_QUADROTOR, MAV_AUTOPILOT_GENERIC, E_HB_BASE_MODE, 0, E_HB_STATUS);
    append_msg(&msg);
    emit_stream("fix_2d.bin");

    // --- emergency: HEARTBEAT (critical) + HEARTBEAT (flight termination)
    reset_stream();
    mavlink_msg_heartbeat_pack(SYS_ID, COMP_ID, &msg,
        MAV_TYPE_QUADROTOR, MAV_AUTOPILOT_GENERIC, B_HB_BASE_MODE, 0, MAV_STATE_CRITICAL);
    append_msg(&msg);
    mavlink_msg_heartbeat_pack(SYS_ID, COMP_ID, &msg,
        MAV_TYPE_QUADROTOR, MAV_AUTOPILOT_GENERIC, B_HB_BASE_MODE, 0, MAV_STATE_FLIGHT_TERMINATION);
    append_msg(&msg);
    emit_stream("emergency.bin");

    // --- undeclared: HEARTBEAT (poweroff - an unmapped status)
    reset_stream();
    mavlink_msg_heartbeat_pack(SYS_ID, COMP_ID, &msg,
        MAV_TYPE_QUADROTOR, MAV_AUTOPILOT_GENERIC, 0, 0, MAV_STATE_POWEROFF);
    append_msg(&msg);
    emit_stream("undeclared.bin");

    return 0;
}
