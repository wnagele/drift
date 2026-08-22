#ifndef DRIFT_DRI_H
#define DRIFT_DRI_H

#include <stddef.h>
#include <stdint.h>
#include <opendroneid.h>

#define DRI_INTERVAL 20         // ms
#define DRI_GUARD_MULTIPLIER 2  // ensure there is sufficient time to broadcast
#define DRI_SCHEDULE_PERIOD (1000 / DRI_INTERVAL / DRI_GUARD_MULTIPLIER)

#define DRI_UUID 0xFFFA         // ASTM
#define DRI_APP_CODE 0x0D       // RD

// Message slot identifiers for the broadcast schedule.
#define DRI_SLOT_BASIC_ID 0
#define DRI_SLOT_SELF_ID 1
#define DRI_SLOT_OPERATOR_ID 2
#define DRI_SLOT_SYSTEM 3
#define DRI_SLOT_LOCATION 4

// Scheduler/broadcast logic (platform-neutral, natively testable).
bool dri_due(unsigned long last_due, unsigned long now);
uint8_t dri_counter_next(uint8_t schedule_counter);
uint8_t dri_slot_type(uint8_t schedule_counter);
// Encode one schedule slot. Returns false when the vendored encoder rejects
// the data (e.g. out-of-range telemetry): the caller must then skip the slot
// instead of broadcasting the pre-zeroed buffer.
bool dri_encode_slot(ODID_UAS_Data *data, uint8_t schedule_counter, ODID_Message_encoded *out);
size_t dri_build_service_data(uint8_t msg_counter, const ODID_Message_encoded *encoded, uint8_t *out_buf);
void dri_populate_identity(ODID_UAS_Data *data, const char *ua_id, const char *op_id, const char *ua_desc);

void dri_init(ODID_UAS_Data *data, unsigned long now);
void dri_transmit(ODID_UAS_Data *data, unsigned long now);
void dri_update_status(ODID_UAS_Data *data, ODID_status_t status);
void dri_update_location(
    ODID_UAS_Data *data,
    double latitude,
    double longitude,
    double alt,
    double relative_alt,
    float heading
);
void dri_update_operator(
    ODID_UAS_Data *data,
    double lat,
    double lon,
    float alt
);

#endif
