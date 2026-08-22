// Generates the ODID test fixtures shared by the native unit tests and the
// e2e suite: one reference encoding per message in test/fixtures/odid/<name>.bin
// (basic_id, self_id, operator_id, system, location), produced by the vendored
// opendroneid library. The scenario below is this generator's own - the
// consuming tests build the same ODID_UAS_Data from their own in-lined values,
// so a change here needs a matching change there.
// Regenerate via `make fixtures` after changes to the vendored library.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <opendroneid.h>

// The reference scenario: a configured aircraft with good telemetry.
static const char UA_ID[] = "1DRIFT000TEST00001";
static const char OP_ID[] = "TEST-OPERATOR-1";
static const char UA_DESC[] = "DRIFT test fixture";
static const ODID_status_t STATUS = ODID_STATUS_AIRBORNE;
static const double LAT = 47.3566123;
static const double LON = 8.5321456;
static const double ALT_GEO = 500.5;
static const double HEIGHT = 30.5;
static const float DIRECTION = 90.0;
static const double OPERATOR_LAT = 47.3566000;
static const double OPERATOR_LON = 8.5321000;
static const double OPERATOR_ALT_GEO = 500.0;

static void emit_message(const char *name, const uint8_t *data, size_t len) {
    char path[256];
    snprintf(path, sizeof path, "test/fixtures/odid/%s.bin", name);
    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "cannot open %s (run from repo root)\n", path);
        exit(1);
    }
    fwrite(data, 1, len, f);
    fclose(f);
    printf("wrote %s (%zu bytes)\n", path, (size_t)len);
}

int main() {
    ODID_UAS_Data data;
    odid_initUasData(&data);

    // Mirrors dri_populate_identity(&data, UA_ID, OP_ID, UA_DESC).
    data.BasicID[0].IDType = ODID_IDTYPE_SERIAL_NUMBER;
    strncpy(data.BasicID[0].UASID, UA_ID, ODID_ID_SIZE);
    data.OperatorID.OperatorIdType = ODID_OPERATOR_ID;
    strncpy(data.OperatorID.OperatorId, OP_ID, ODID_ID_SIZE);
    data.SelfID.DescType = ODID_DESC_TYPE_TEXT;
    strncpy(data.SelfID.Desc, UA_DESC, ODID_STR_SIZE);

    // Mirrors dri_update_status / dri_update_location / dri_update_operator.
    data.Location.Status = STATUS;
    data.Location.Latitude = LAT;
    data.Location.Longitude = LON;
    data.Location.AltitudeGeo = ALT_GEO;
    data.Location.HeightType = ODID_HEIGHT_REF_OVER_TAKEOFF;
    data.Location.Height = HEIGHT;
    data.Location.Direction = DIRECTION;
    data.System.OperatorLocationType = ODID_OPERATOR_LOCATION_TYPE_TAKEOFF;
    data.System.OperatorLatitude = OPERATOR_LAT;
    data.System.OperatorLongitude = OPERATOR_LON;
    data.System.OperatorAltitudeGeo = OPERATOR_ALT_GEO;

    ODID_Message_encoded encoded;

    memset(&encoded, 0, sizeof(encoded));
    encodeBasicIDMessage((ODID_BasicID_encoded *)&encoded, &data.BasicID[0]);
    emit_message("basic_id", encoded.rawData, ODID_MESSAGE_SIZE);

    memset(&encoded, 0, sizeof(encoded));
    encodeSelfIDMessage((ODID_SelfID_encoded *)&encoded, &data.SelfID);
    emit_message("self_id", encoded.rawData, ODID_MESSAGE_SIZE);

    memset(&encoded, 0, sizeof(encoded));
    encodeOperatorIDMessage((ODID_OperatorID_encoded *)&encoded, &data.OperatorID);
    emit_message("operator_id", encoded.rawData, ODID_MESSAGE_SIZE);

    memset(&encoded, 0, sizeof(encoded));
    encodeSystemMessage((ODID_System_encoded *)&encoded, &data.System);
    emit_message("system", encoded.rawData, ODID_MESSAGE_SIZE);

    memset(&encoded, 0, sizeof(encoded));
    encodeLocationMessage((ODID_Location_encoded *)&encoded, &data.Location);
    emit_message("location", encoded.rawData, ODID_MESSAGE_SIZE);

    // The armed_unknown MAVLink scenario: same aircraft, but heading unknown
    // (INV_DIR) and altitude/height unknown (INV_ALT) after main.cpp's range
    // guards. The encoder accepts these spec "unknown" sentinels - unlike the
    // raw 655.35 deg / 2147483 m values, which it rejects outright.
    data.Location.Direction = INV_DIR;
    data.Location.AltitudeGeo = INV_ALT;
    data.Location.Height = INV_ALT;
    memset(&encoded, 0, sizeof(encoded));
    encodeLocationMessage((ODID_Location_encoded *)&encoded, &data.Location);
    emit_message("location_unknown", encoded.rawData, ODID_MESSAGE_SIZE);

    return 0;
}
