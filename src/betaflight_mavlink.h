#include <MAVLink.h>

typedef struct {
    mavlink_heartbeat_t heartbeat;
    mavlink_global_position_int_t global_position_int;
    mavlink_gps_raw_int_t gps_raw_int;
    mavlink_gps_global_origin_t gps_global_origin;
} mavlink_state_t;

typedef enum mavlink_type {
    HEARTBEAT = MAVLINK_MSG_ID_HEARTBEAT,
    GLOBAL_POSITION_INT = MAVLINK_MSG_ID_GLOBAL_POSITION_INT,
    GPS_RAW_INT = MAVLINK_MSG_ID_GPS_RAW_INT,
    GPS_GLOBAL_ORIGIN = MAVLINK_MSG_ID_GPS_GLOBAL_ORIGIN,
    NONE,
} mavlink_type_t;

void mavlink_init(mavlink_state_t *state);
mavlink_type_t mavlink_parse(mavlink_state_t *state);