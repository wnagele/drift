#ifndef DRIFT_DRI_H
#define DRIFT_DRI_H

#include <stddef.h>
#include <stdint.h>
#include <opendroneid.h>

#define DRI_INTERVAL 20         // ms
#define DRI_GUARD_MULTIPLIER 2  // ensure there is sufficient time to broadcast
#define DRI_SCHEDULE_PERIOD (1000 / DRI_INTERVAL / DRI_GUARD_MULTIPLIER)

// The BLE5 Long Range transport broadcasts a Message Pack (ODID message type
// 0xF): every currently valid message combined into one payload, on its own
// ~1 s cadence alongside the per-message slot schedule.
#define DRI_PACK_INTERVAL 1000  // ms between message-pack broadcasts
// Pack header (type/version, single-message size, message count) plus the
// maximum of nine 25-byte messages.
#define DRI_PACK_MAX_SIZE (3 + ODID_PACK_MAX_MESSAGES * ODID_MESSAGE_SIZE)

// The Wi-Fi Beacon transport refreshes the same Message Pack payload in a
// vendor IE on the SoftAP's beacons and probe responses. ~5 Hz keeps the
// Location data a receiver can observe well inside the 1 s freshness rule;
// the SoftAP's own ~100 TU beacon interval repeats the IE between refreshes.
#define DRI_WIFI_BEACON_INTERVAL 200  // ms between Wi-Fi Beacon IE refreshes

// The Wi-Fi NAN transport transmits a NAN sync beacon plus a Message Pack
// action frame every NAN discovery window. The window period is 512 ms
// (16 TU x 32, the NAN spec's cluster timing the reference implementations
// broadcast on), which keeps the observable Location data inside the 1 s
// freshness rule.
#define DRI_WIFI_NAN_INTERVAL 512  // ms between Wi-Fi NAN discovery window broadcasts

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
// Build a Message Pack from everything the UAS data marks valid. Returns the
// encoded length, or <= 0 when the vendored builder rejects the input (an
// empty pack - nothing valid yet - is rejected rather than broadcast).
int dri_build_pack(ODID_UAS_Data *data, uint8_t *out_buf, size_t buflen);
size_t dri_build_pack_service_data(uint8_t msg_counter, const uint8_t *pack, size_t pack_len, uint8_t *out_buf);
void dri_populate_identity(ODID_UAS_Data *data, const char *ua_id, const char *op_id, const char *ua_desc);

bool dri_pack_due(unsigned long last_due, unsigned long now);
bool dri_wifi_beacon_due(unsigned long last_due, unsigned long now);
bool dri_wifi_nan_due(unsigned long last_due, unsigned long now);
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
