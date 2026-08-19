#include <unity.h>

#include "betaflight_mavlink.h"
#include "../support/fixtures.h"

// The fixtures (test/fixtures/mavlink/*.bin, shared with the e2e suite) define
// the MAVLink inputs; the expected decoded values below are owned by these
// tests. Coordinates are MAVLink wire units: lat/lon in 1e7 deg, alt in mm,
// heading in 1e2 deg.

static mavlink_state_t state;

void setUp() {
    // Reset the decode state *and* the parser's global channel state, so a
    // partial frame buffered by one test cannot corrupt another (order
    // independence; see test_partial_frame_then_none).
    mavlink_reset(&state);
}

// Feed a stream byte by byte, recording the message types returned.
static size_t feed(const std::string &data, mavlink_type_t *seen, size_t seen_max) {
    size_t count = 0;
    for (size_t i = 0; i < data.size(); i++) {
        mavlink_type_t t = mavlink_parse_byte(&state, (uint8_t)data[i]);
        if (t != NONE && count < seen_max)
            seen[count++] = t;
    }
    return count;
}

static std::string capture(const char *name) {
    std::string data = fixture_read(std::string("mavlink/") + name + ".bin");
    TEST_ASSERT_FALSE(data.empty());
    return data;
}

void test_disarmed_fix_stream() {
    mavlink_type_t seen[8];
    size_t count = feed(capture("disarmed_fix"), seen, 8);

    TEST_ASSERT_EQUAL(3, count);
    TEST_ASSERT_EQUAL(GPS_RAW_INT, seen[0]);
    TEST_ASSERT_EQUAL(GLOBAL_POSITION_INT, seen[1]);
    TEST_ASSERT_EQUAL(HEARTBEAT, seen[2]);

    TEST_ASSERT_EQUAL(3, state.gps_raw_int.fix_type);              // 3D fix
    TEST_ASSERT_EQUAL(473566000, state.gps_raw_int.lat);           // 47.3566 deg
    TEST_ASSERT_EQUAL(85321000, state.gps_raw_int.lon);            // 8.5321 deg
    TEST_ASSERT_EQUAL(500000, state.gps_raw_int.alt);              // 500 m

    TEST_ASSERT_EQUAL(473566123, state.global_position_int.lat);
    TEST_ASSERT_EQUAL(85321456, state.global_position_int.lon);
    TEST_ASSERT_EQUAL(500500, state.global_position_int.alt);      // 500.5 m
    TEST_ASSERT_EQUAL(30500, state.global_position_int.relative_alt);  // 30.5 m
    TEST_ASSERT_EQUAL(9000, state.global_position_int.hdg);        // 90 deg

    TEST_ASSERT_EQUAL(MAV_STATE_STANDBY, state.heartbeat.system_status);
}

void test_armed_no_fix_stream() {
    mavlink_type_t seen[8];
    size_t count = feed(capture("armed_no_fix"), seen, 8);

    TEST_ASSERT_EQUAL(2, count);
    TEST_ASSERT_EQUAL(GPS_RAW_INT, seen[0]);
    TEST_ASSERT_EQUAL(HEARTBEAT, seen[1]);

    TEST_ASSERT_EQUAL(1, state.gps_raw_int.fix_type);              // no fix
    TEST_ASSERT_EQUAL(MAV_STATE_ACTIVE, state.heartbeat.system_status);
    TEST_ASSERT_EQUAL(MAV_MODE_FLAG_SAFETY_ARMED, state.heartbeat.base_mode);
}

void test_armed_fix_stream() {
    // The primary real-world scenario: an armed aircraft with good telemetry.
    mavlink_type_t seen[8];
    size_t count = feed(capture("armed_fix"), seen, 8);

    TEST_ASSERT_EQUAL(3, count);
    TEST_ASSERT_EQUAL(GPS_RAW_INT, seen[0]);
    TEST_ASSERT_EQUAL(HEARTBEAT, seen[1]);
    TEST_ASSERT_EQUAL(GLOBAL_POSITION_INT, seen[2]);

    TEST_ASSERT_EQUAL(3, state.gps_raw_int.fix_type);              // 3D fix
    TEST_ASSERT_EQUAL(MAV_STATE_ACTIVE, state.heartbeat.system_status);
    TEST_ASSERT_EQUAL(MAV_MODE_FLAG_SAFETY_ARMED, state.heartbeat.base_mode);
    TEST_ASSERT_EQUAL(473566123, state.global_position_int.lat);
    TEST_ASSERT_EQUAL(85321456, state.global_position_int.lon);
    TEST_ASSERT_EQUAL(500500, state.global_position_int.alt);      // 500.5 m
    TEST_ASSERT_EQUAL(30500, state.global_position_int.relative_alt);  // 30.5 m
    TEST_ASSERT_EQUAL(9000, state.global_position_int.hdg);        // 90 deg
}

void test_armed_unknown_stream() {
    // Heading "unknown" (hdg = UINT16_MAX, the MAVLink sentinel) and
    // altitudes near INT32_MAX (mm): decodable values that have no ODID
    // representation. main.cpp maps them to the ODID unknown sentinels.
    mavlink_type_t seen[8];
    size_t count = feed(capture("armed_unknown"), seen, 8);

    TEST_ASSERT_EQUAL(3, count);
    TEST_ASSERT_EQUAL(GPS_RAW_INT, seen[0]);
    TEST_ASSERT_EQUAL(HEARTBEAT, seen[1]);
    TEST_ASSERT_EQUAL(GLOBAL_POSITION_INT, seen[2]);

    TEST_ASSERT_EQUAL(3, state.gps_raw_int.fix_type);              // 3D fix
    TEST_ASSERT_EQUAL(MAV_STATE_ACTIVE, state.heartbeat.system_status);
    TEST_ASSERT_EQUAL(UINT16_MAX, state.global_position_int.hdg);  // unknown
    TEST_ASSERT_EQUAL(INT32_MAX, state.global_position_int.alt);
    TEST_ASSERT_EQUAL(INT32_MAX, state.global_position_int.relative_alt);
}

void test_no_fix_position_stream() {
    // A position and an origin arriving without any GNSS fix; main.cpp must
    // gate both on gps_fix (asserted by the e2e branches scenarios).
    mavlink_type_t seen[8];
    size_t count = feed(capture("no_fix_position"), seen, 8);

    TEST_ASSERT_EQUAL(4, count);
    TEST_ASSERT_EQUAL(GPS_RAW_INT, seen[0]);
    TEST_ASSERT_EQUAL(GLOBAL_POSITION_INT, seen[1]);
    TEST_ASSERT_EQUAL(GPS_GLOBAL_ORIGIN, seen[2]);
    TEST_ASSERT_EQUAL(HEARTBEAT, seen[3]);

    TEST_ASSERT_EQUAL(1, state.gps_raw_int.fix_type);              // no fix
    TEST_ASSERT_EQUAL(470000000, state.global_position_int.lat);   // 47.0 deg
    TEST_ASSERT_EQUAL(80000000, state.global_position_int.lon);    // 8.0 deg
    TEST_ASSERT_EQUAL(470000000, state.gps_global_origin.latitude);
    TEST_ASSERT_EQUAL(123000, state.gps_global_origin.altitude);
    TEST_ASSERT_EQUAL(MAV_STATE_ACTIVE, state.heartbeat.system_status);
    TEST_ASSERT_EQUAL(MAV_MODE_FLAG_SAFETY_ARMED, state.heartbeat.base_mode);
}

void test_fix_2d_stream() {
    // A 2D fix does not count: fix_type 2 is below the 3D threshold.
    mavlink_type_t seen[8];
    size_t count = feed(capture("fix_2d"), seen, 8);

    TEST_ASSERT_EQUAL(3, count);
    TEST_ASSERT_EQUAL(GPS_RAW_INT, seen[0]);
    TEST_ASSERT_EQUAL(GLOBAL_POSITION_INT, seen[1]);
    TEST_ASSERT_EQUAL(HEARTBEAT, seen[2]);

    TEST_ASSERT_EQUAL(2, state.gps_raw_int.fix_type);              // 2D fix
    TEST_ASSERT_EQUAL(470000000, state.global_position_int.lat);
}

void test_emergency_stream() {
    // Failsafe heartbeats: critical, then flight termination.
    mavlink_type_t seen[8];
    size_t count = feed(capture("emergency"), seen, 8);

    TEST_ASSERT_EQUAL(2, count);
    TEST_ASSERT_EQUAL(HEARTBEAT, seen[0]);
    TEST_ASSERT_EQUAL(HEARTBEAT, seen[1]);
    TEST_ASSERT_EQUAL(MAV_STATE_FLIGHT_TERMINATION, state.heartbeat.system_status);
}

void test_undeclared_stream() {
    // An unmapped status (poweroff): decodes fine, maps to UNDECLARED.
    mavlink_type_t seen[8];
    size_t count = feed(capture("undeclared"), seen, 8);

    TEST_ASSERT_EQUAL(1, count);
    TEST_ASSERT_EQUAL(HEARTBEAT, seen[0]);
    TEST_ASSERT_EQUAL(MAV_STATE_POWEROFF, state.heartbeat.system_status);
    TEST_ASSERT_EQUAL(0, state.heartbeat.base_mode);               // disarmed
}

void test_origin_set_stream() {
    mavlink_type_t seen[8];
    size_t count = feed(capture("origin_set"), seen, 8);

    TEST_ASSERT_EQUAL(2, count);
    TEST_ASSERT_EQUAL(GPS_RAW_INT, seen[0]);
    TEST_ASSERT_EQUAL(GPS_GLOBAL_ORIGIN, seen[1]);

    TEST_ASSERT_EQUAL(473566000, state.gps_global_origin.latitude);
    TEST_ASSERT_EQUAL(85321000, state.gps_global_origin.longitude);
    TEST_ASSERT_EQUAL(500000, state.gps_global_origin.altitude);
}

void test_garbage_resync() {
    // Noise bytes before a valid stream must be skipped (NONE) and the
    // following frames must still decode. (Noise deliberately avoids 0xFE,
    // the MAVLink v1 start-of-text byte.)
    const uint8_t noise[] = {0xFF, 0x00, 0x13, 0x37, 0x42};
    mavlink_type_t seen[8];
    for (size_t i = 0; i < sizeof(noise); i++)
        TEST_ASSERT_EQUAL(NONE, mavlink_parse_byte(&state, noise[i]));
    TEST_ASSERT_EQUAL(3, feed(capture("disarmed_fix"), seen, 8));
    TEST_ASSERT_EQUAL(MAV_STATE_STANDBY, state.heartbeat.system_status);
}

void test_partial_frame_then_none() {
    // A frame split across reads is buffered, not dropped: all but the last
    // byte yield NONE, and the final byte completes the message.
    std::string stream = capture("disarmed_fix");
    size_t frame_len = 6 + (uint8_t)stream[1] + 2; // header + payload + crc
    for (size_t i = 0; i < frame_len - 1; i++)
        TEST_ASSERT_EQUAL(NONE, mavlink_parse_byte(&state, (uint8_t)stream[i]));
    TEST_ASSERT_EQUAL(GPS_RAW_INT, mavlink_parse_byte(&state, (uint8_t)stream[frame_len - 1]));
    TEST_ASSERT_EQUAL(3, state.gps_raw_int.fix_type);
    TEST_ASSERT_EQUAL(473566000, state.gps_raw_int.lat);

    // Then deliberately leave a partial frame buffered in the parser: this
    // test runs first (see main), so the next test only passes if setUp()
    // resets the channel state.
    for (size_t i = 0; i < frame_len - 1; i++)
        TEST_ASSERT_EQUAL(NONE, mavlink_parse_byte(&state, (uint8_t)stream[i]));
}

int main(int, char **) {
    UNITY_BEGIN();
    // The frame-corrupting test runs first: it used to pass only because it
    // ran last, leaving its partial frame behind for nobody.
    RUN_TEST(test_partial_frame_then_none);
    RUN_TEST(test_disarmed_fix_stream);
    RUN_TEST(test_armed_no_fix_stream);
    RUN_TEST(test_armed_fix_stream);
    RUN_TEST(test_armed_unknown_stream);
    RUN_TEST(test_no_fix_position_stream);
    RUN_TEST(test_fix_2d_stream);
    RUN_TEST(test_emergency_stream);
    RUN_TEST(test_undeclared_stream);
    RUN_TEST(test_origin_set_stream);
    RUN_TEST(test_garbage_resync);
    return UNITY_END();
}
