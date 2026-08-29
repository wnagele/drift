#include <string.h>
#include "ble.h"
#include "dri.h"

static ODID_Message_encoded encoded;
static uint8_t msg_counter = 0;
static uint8_t schedule_counter = 0;
static unsigned long last = 0;

static uint8_t pack[DRI_PACK_MAX_SIZE];
static uint8_t pack_counter = 0;
static unsigned long last_pack = 0;

bool dri_due(unsigned long last_due, unsigned long now) {
    return now - last_due > DRI_INTERVAL * DRI_GUARD_MULTIPLIER;
}

bool dri_pack_due(unsigned long last_due, unsigned long now) {
    return now - last_due > DRI_PACK_INTERVAL;
}

uint8_t dri_counter_next(uint8_t schedule) {
    return schedule >= DRI_SCHEDULE_PERIOD ? 1 : schedule + 1;
}

uint8_t dri_slot_type(uint8_t schedule) {
    switch (schedule) {
        case 1:
            return DRI_SLOT_BASIC_ID;
        case 2:
            return DRI_SLOT_SELF_ID;
        case 3:
            return DRI_SLOT_OPERATOR_ID;
        case 4:
            return DRI_SLOT_SYSTEM;
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
}

void dri_transmit(ODID_UAS_Data *data, unsigned long now) {
    if (dri_pack_due(last_pack, now)) {
        last_pack = now;
        int pack_len = dri_build_pack(data, pack, sizeof(pack));
        // An empty pack (nothing valid) is rejected by the builder: skip the
        // slot rather than broadcast an empty advertisement.
        if (pack_len > 0)
            ble_send_pack(pack_counter++, pack, pack_len);
    }

    if (!dri_due(last, now))
        return;
    last = now;

    schedule_counter = dri_counter_next(schedule_counter);
    if (!dri_encode_slot(data, schedule_counter, &encoded))
        return;  // encoder rejected the data: skip the slot rather than broadcast an empty message
    ble_send(msg_counter++, &encoded);
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
