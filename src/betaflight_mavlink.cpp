#include "betaflight_mavlink.h"
#include <HardwareSerial.h>

#define MAVLINK_CHANNEL MAVLINK_COMM_0
#define MAVLINK_BAUD_RATE 9600
#define MAVLinkSerial Serial0

void mavlink_init(mavlink_state_t *state) {
    MAVLinkSerial.begin(MAVLINK_BAUD_RATE);
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

mavlink_type_t mavlink_parse(mavlink_state_t *state) {
    if (!MAVLinkSerial.available())
        return NONE;
    return mavlink_parse_byte(state, MAVLinkSerial.read());
}