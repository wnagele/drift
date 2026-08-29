#include <string.h>
#include "ble.h"
#include "dri.h"
#include "txcount.h"
#include "wifi_beacon.h"
#include "wifi_nan.h"

static ODID_Message_encoded encoded;
static uint8_t msg_counter = 0;
static uint8_t schedule_counter = 0;
static unsigned long last = 0;

static uint8_t pack[DRI_PACK_MAX_SIZE];
static uint8_t pack_counter = 0;
static unsigned long last_pack = 0;

static uint8_t wifi_beacon_pack[DRI_PACK_MAX_SIZE];
static uint8_t wifi_beacon_counter = 0;
static unsigned long last_wifi_beacon = 0;

static uint8_t wifi_nan_frame[WIFI_NAN_FRAME_MAX_SIZE];
static uint8_t wifi_nan_counter = 0;
static unsigned long last_wifi_nan = 0;

bool dri_due(unsigned long last_due, unsigned long now) {
    return now - last_due > DRI_SLOT_INTERVAL;
}

bool dri_pack_due(unsigned long last_due, unsigned long now) {
    return now - last_due > DRI_PACK_INTERVAL;
}

bool dri_wifi_beacon_due(unsigned long last_due, unsigned long now) {
    return now - last_due > DRI_WIFI_BEACON_INTERVAL;
}

bool dri_wifi_nan_due(unsigned long last_due, unsigned long now) {
    return now - last_due > DRI_WIFI_NAN_INTERVAL;
}

uint8_t dri_counter_next(uint8_t schedule) {
    return schedule >= DRI_SCHEDULE_PERIOD ? 1 : schedule + 1;
}

uint8_t dri_slot_type(uint8_t schedule) {
    // The 10-slot cycle allocates the F3411 message rates with margin
    // (Location 2.5 Hz; Basic ID and System 2 Hz — the FAA/Japan 1 Hz
    // requirement; Self-ID and Operator ID 1 Hz against the 3 s baseline):
    //
    //   slot:    1     2        3      4        5       6        7     8        9      10
    //   message: Basic Loc    System Loc    SelfID  Loc    Basic Loc    System OpID
    switch (schedule) {
        case 1:
        case 7:
            return DRI_SLOT_BASIC_ID;
        case 3:
        case 9:
            return DRI_SLOT_SYSTEM;
        case 5:
            return DRI_SLOT_SELF_ID;
        case 10:
            return DRI_SLOT_OPERATOR_ID;
        default:
            return DRI_SLOT_LOCATION;
    }
}

bool dri_encode_slot(ODID_UAS_Data *data, uint8_t schedule, ODID_Message_encoded *out) {
    memset(out, 0, sizeof(ODID_Message_encoded));
    int rc;
    switch (dri_slot_type(schedule)) {
        case DRI_SLOT_BASIC_ID:
            rc = encodeBasicIDMessage((ODID_BasicID_encoded*) out, &data->BasicID[0]);
            break;
        case DRI_SLOT_SELF_ID:
            rc = encodeSelfIDMessage((ODID_SelfID_encoded*) out, &data->SelfID);
            break;
        case DRI_SLOT_OPERATOR_ID:
            rc = encodeOperatorIDMessage((ODID_OperatorID_encoded*) out, &data->OperatorID);
            break;
        case DRI_SLOT_SYSTEM:
            rc = encodeSystemMessage((ODID_System_encoded*) out, &data->System);
            break;
        default:
            rc = encodeLocationMessage((ODID_Location_encoded*) out, &data->Location);
            break;
    }
    return rc == ODID_SUCCESS;
}

size_t dri_build_service_data(uint8_t msg_counter, const ODID_Message_encoded *enc, uint8_t *out_buf) {
    memset(out_buf, 0, ODID_MESSAGE_SIZE + 2);
    out_buf[0] = (uint8_t)DRI_APP_CODE;
    out_buf[1] = msg_counter;
    for (int i = 0; i < ODID_MESSAGE_SIZE; i++)
        out_buf[2 + i] = enc->rawData[i];
    return ODID_MESSAGE_SIZE + 2;
}

int dri_build_pack(ODID_UAS_Data *data, uint8_t *out_buf, size_t buflen) {
    return odid_message_build_pack(data, out_buf, buflen);
}

size_t dri_build_pack_service_data(uint8_t msg_counter, const uint8_t *pack, size_t pack_len, uint8_t *out_buf) {
    memset(out_buf, 0, pack_len + 2);
    out_buf[0] = (uint8_t)DRI_APP_CODE;
    out_buf[1] = msg_counter;
    for (size_t i = 0; i < pack_len; i++)
        out_buf[2 + i] = pack[i];
    return pack_len + 2;
}

void dri_populate_identity(ODID_UAS_Data *data, const char *ua_id, const char *op_id, const char *ua_desc) {
    if (ua_id[0] != '\0' && strlen(ua_id) <= ODID_ID_SIZE) {
        data->BasicID[0].IDType = ODID_IDTYPE_SERIAL_NUMBER;
        strncpy(data->BasicID[0].UASID, ua_id, ODID_ID_SIZE);
        data->BasicIDValid[0] = 1;
    }
    if (op_id[0] != '\0' && strlen(op_id) <= ODID_ID_SIZE) {
        data->OperatorID.OperatorIdType = ODID_OPERATOR_ID;
        strncpy(data->OperatorID.OperatorId, op_id, ODID_ID_SIZE);
        data->OperatorIDValid = 1;
    }
    if (ua_desc[0] != '\0' && strlen(ua_desc) <= ODID_STR_SIZE) {
        data->SelfID.DescType = ODID_DESC_TYPE_TEXT;
        strncpy(data->SelfID.Desc, ua_desc, ODID_STR_SIZE);
        data->SelfIDValid = 1;
    }
}

void dri_init(ODID_UAS_Data *data, unsigned long now) {
    odid_initUasData(data);

    /*
    data->BasicID[0].UAType = ODID_UATYPE_HELICOPTER_OR_MULTIROTOR;
    */

    /*
    data->System.ClassificationType = ODID_CLASSIFICATION_TYPE_EU;
    data->System.CategoryEU = ODID_CATEGORY_EU_OPEN;
    data->System.ClassEU = ODID_CLASS_EU_CLASS_0;
    */

    /*
    data->System.AreaCount = 1;
    data->System.AreaRadius = 500;
    data->System.AreaFloor = 0;
    data->System.AreaCeiling = 50;
    */

    // The message pack (BLE5 transport) is composed from the *Valid flags.
    // The location is part of the broadcast from power-on exactly like the
    // BT4 schedule's location slots - odid_initUasData() has already filled
    // it with the ODID "unknown" sentinels - while the identity and system
    // messages only join once their data arrives (dri_populate_identity(),
    // dri_update_operator()).
    data->LocationValid = 1;

    msg_counter = 0;
    schedule_counter = 0;
    last = now;
    pack_counter = 0;
    last_pack = now;
    wifi_beacon_counter = 0;
    last_wifi_beacon = now;
    wifi_nan_counter = 0;
    last_wifi_nan = now;
}

void dri_transmit(ODID_UAS_Data *data, unsigned long now) {
    if (dri_wifi_beacon_due(last_wifi_beacon, now)) {
        last_wifi_beacon = now;
        int pack_len = dri_build_pack(data, wifi_beacon_pack, sizeof(wifi_beacon_pack));
        // An empty pack (nothing valid) is rejected by the builder: skip the
        // refresh rather than register an empty IE.
        if (pack_len > 0) {
            wifi_beacon_send_pack(wifi_beacon_counter++, wifi_beacon_pack, pack_len);
            txcount_beacon((size_t)pack_len);
        }
    }

    if (dri_wifi_nan_due(last_wifi_nan, now)) {
        last_wifi_nan = now;
        // Every discovery window carries the sync beacon that keeps the NAN
        // cluster alive for receivers scanning channel 6; it holds no ODID
        // data and goes out even when the action frame below is skipped.
        int len = odid_wifi_build_nan_sync_beacon_frame(
            (const char *)wifi_nan_mac(), wifi_nan_frame, sizeof(wifi_nan_frame));
        if (len > 0)
            wifi_nan_send_sync_beacon(wifi_nan_frame, len);
        // The action frame wraps the same Message Pack the other transports
        // broadcast; an empty pack (nothing valid) is rejected by the builder,
        // so the frame is skipped rather than transmitted empty - and the
        // counter only advances once a frame actually went out.
        // The pack is rebuilt into the BT5 buffer purely to count its
        // messages for the transmit diagnostics (the vendored builder is the
        // authority on which messages are valid); the BT5 block below
        // rebuilds that buffer in full before its own send, so sharing it is
        // safe even if the block order ever changes.
        int msg_count_len = dri_build_pack(data, pack, sizeof(pack));
        len = odid_wifi_build_message_pack_nan_action_frame(
            data, (const char *)wifi_nan_mac(), wifi_nan_counter,
            wifi_nan_frame, sizeof(wifi_nan_frame));
        if (len > 0) {
            wifi_nan_send_action_frame(wifi_nan_frame, len);
            wifi_nan_counter++;
            if (msg_count_len > 0)
                txcount_nan((size_t)msg_count_len);
        }
    }

    if (dri_pack_due(last_pack, now)) {
        last_pack = now;
        int pack_len = dri_build_pack(data, pack, sizeof(pack));
        // An empty pack (nothing valid) is rejected by the builder: skip the
        // slot rather than broadcast an empty advertisement.
        if (pack_len > 0) {
            ble_send_pack(pack_counter++, pack, pack_len);
            txcount_bt5((size_t)pack_len);
        }
    }

    if (!dri_due(last, now))
        return;
    last = now;

    schedule_counter = dri_counter_next(schedule_counter);
    if (!dri_encode_slot(data, schedule_counter, &encoded))
        return;  // encoder rejected the data: skip the slot rather than broadcast an empty message
    ble_send(msg_counter++, &encoded);
    txcount_bt4();
}

void dri_update_status(ODID_UAS_Data *data, ODID_status_t status) {
    data->Location.Status = status;
}

void dri_update_location(
    ODID_UAS_Data *data,
    double lat,
    double lon,
    double alt,
    double rel_alt,
    float hdg
) {
    data->Location.Latitude = lat;
    data->Location.Longitude = lon;
    data->Location.AltitudeGeo = alt;
    data->Location.HeightType = ODID_HEIGHT_REF_OVER_TAKEOFF;
    data->Location.Height = rel_alt;
    data->Location.Direction = hdg;
    data->LocationValid = 1;
}

void dri_update_operator(
    ODID_UAS_Data *data,
    double lat,
    double lon,
    float alt
) {
    data->System.OperatorLocationType = ODID_OPERATOR_LOCATION_TYPE_TAKEOFF;
    data->System.OperatorLatitude = lat;
    data->System.OperatorLongitude = lon;
    data->System.OperatorAltitudeGeo = alt;
    data->SystemValid = 1;
}
