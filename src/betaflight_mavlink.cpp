#include "betaflight_mavlink.h"

#include <string.h>

#define MAVLINK_CHANNEL MAVLINK_COMM_0

void mavlink_reset(mavlink_state_t *state) {
    // Reset both the caller's decode state and the MAVLink library's global
    // channel state: a partial frame left in the parser's buffer by one test
    // would otherwise corrupt the next one (mavlink_parse_byte keeps its
    // parse progress in MAVLINK_COMM_0, not in the state struct).
    memset(state, 0, sizeof(*state));
    memset(mavlink_get_channel_status(MAVLINK_CHANNEL), 0, sizeof(mavlink_status_t));
}

mavlink_type_t mavlink_parse_byte(mavlink_state_t *state, uint8_t data) {
    mavlink_message_t message;
    mavlink_status_t status;

    if (mavlink_parse_char(MAVLINK_CHANNEL, data, &message, &status)) {
        switch (message.msgid) {
            case MAVLINK_MSG_ID_HEARTBEAT:
                mavlink_msg_heartbeat_decode(&message, &state->heartbeat);
                return HEARTBEAT;
            case MAVLINK_MSG_ID_GLOBAL_POSITION_INT:
                mavlink_msg_global_position_int_decode(&message, &state->global_position_int);
                return GLOBAL_POSITION_INT;
            case MAVLINK_MSG_ID_GPS_RAW_INT:
                mavlink_msg_gps_raw_int_decode(&message, &state->gps_raw_int);
                return GPS_RAW_INT;
            case MAVLINK_MSG_ID_GPS_GLOBAL_ORIGIN:
                mavlink_msg_gps_global_origin_decode(&message, &state->gps_global_origin);
                return GPS_GLOBAL_ORIGIN;
        }
    }
    return NONE;
}

// --- Serial source seam ------------------------------------------------------
// The only platform-dependent part of the module: which UART the telemetry
// arrives on, configured per build in platformio.ini. Everything above is
// platform-neutral.

#if defined(ESP32)
#include <HardwareSerial.h>

#define MAVLINK_BAUD_RATE 9600

#if defined(DRIFT_MAVLINK_UART1)
// STDOUT is UART0 (Serial), telemetry on UART1 (e2e/QEMU build).
#define MAVLinkSerial Serial1
#elif defined(DRIFT_MAVLINK_UART0)
// STDOUT is the USB CDC console (Serial), telemetry on UART0 (device build).
#define MAVLinkSerial Serial0
#else
#error "DRIFT_MAVLINK_UART0 or DRIFT_MAVLINK_UART1 must be defined (see platformio.ini)"
#endif

static void mavlink_serial_init() {
    MAVLinkSerial.begin(MAVLINK_BAUD_RATE);
}

static int mavlink_serial_read() {
    if (!MAVLinkSerial.available())
        return -1;
    return MAVLinkSerial.read();
}

#else // no serial source (native tests feed bytes via mavlink_parse_byte)

static void mavlink_serial_init() {}

static int mavlink_serial_read() {
    return -1;
}

#endif

void mavlink_init(mavlink_state_t *state) {
    (void) state;
    mavlink_serial_init();
}

mavlink_type_t mavlink_parse(mavlink_state_t *state) {
    int data = mavlink_serial_read();
    if (data < 0)
        return NONE;
    return mavlink_parse_byte(state, (uint8_t) data);
}
