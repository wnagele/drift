#include <ArduinoJson.h>

#include "broadcast.h"

// The enum vocabulary, in ordinal order. These strings are the API (see
// broadcast.h): the dash prints them verbatim and holds no table of its own.
// Each one is the standard's own name for the value, shortened only where
// the standard spells a unit out (the accuracy bounds).
static const char *const UAS_ID_TYPES[] = {
    "None", "Serial Number", "CAA Registration ID", "UTM Assigned UUID",
    "Specific Session ID",
};

static const char *const UA_TYPES[] = {
    "None", "Aeroplane", "Helicopter or Multirotor", "Gyroplane",
    "Hybrid Lift", "Ornithopter", "Glider", "Kite", "Free Balloon",
    "Captive Balloon", "Airship", "Free Fall / Parachute", "Rocket",
    "Tethered Powered Aircraft", "Ground Obstacle", "Other",
};

static const char *const FLIGHT_STATUSES[] = {
    "Undeclared", "Ground", "Airborne", "Emergency",
    "Remote ID System Failure",
};

static const char *const HEIGHT_TYPES[] = {
    "Above Take-off", "Above Ground",
};

static const char *const HORIZ_ACCURACIES[] = {
    "Unknown", "< 10 NM", "< 4 NM", "< 2 NM", "< 1 NM", "< 0.5 NM",
    "< 0.3 NM", "< 0.1 NM", "< 0.05 NM", "< 30 m", "< 10 m", "< 3 m",
    "< 1 m",
};

static const char *const VERT_ACCURACIES[] = {
    "Unknown", "< 150 m", "< 45 m", "< 25 m", "< 10 m", "< 3 m", "< 1 m",
};

static const char *const SPEED_ACCURACIES[] = {
    "Unknown", "< 10 m/s", "< 3 m/s", "< 1 m/s", "< 0.3 m/s",
};

static const char *const OPERATOR_LOCATION_TYPES[] = {
    "Take-off", "Live GNSS", "Fixed",
};

static const char *const OPERATOR_ID_TYPES[] = {
    "Operator ID",
};

static const char *const DESC_TYPES[] = {
    "Text", "Emergency", "Extended Status",
};

#define ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))

// Every one of these enums has reserved gaps in the standard (uas_id_type
// 5-15, flight_status 5-15, desc_type 3-255, ...). A receiver would render
// an unknown ordinal as *something*, so the panel must too: a blank would
// read as a serializer bug. DRIFT cannot currently produce one - every
// value it writes is hardcoded or range-checked by the vendored encoders -
// which is exactly why this needs a test rather than a hope.
static void put_enum(JsonDocument &doc, const char *key,
                     const char *const *names, size_t count, int value) {
    if (value >= 0 && (size_t)value < count)
        doc[key] = names[value];
    else
        doc[key] = String("Reserved (") + value + ")";   // copied, not linked
}

// A measurement, or null when the struct holds ODID's in-band "no value"
// (opendroneid.h: INV_DIR 361, INV_SPEED_H 255, INV_SPEED_V 63,
// INV_ALT -1000). The comparison is exact because these are the sentinel
// constants themselves, assigned by main.cpp's range guards and by
// odid_initLocationData() rather than arrived at by arithmetic.
static void put_number(JsonDocument &doc, const char *key, double value,
                       double no_value) {
    if (value == no_value)
        doc[key] = nullptr;
    else
        doc[key] = value;
}

// ODID's unknown for a position is *both* coordinates at zero, so the test
// is a pair test, per the standard - which means a genuine position at
// exactly 0 deg, 0 deg is indistinguishable from "no position". That is the
// standard's limitation, not ours: do not "fix" it by testing the fields
// independently.
static void put_position(JsonDocument &doc, const char *lat_key,
                         const char *lon_key, double lat, double lon) {
    if (lat == 0.0 && lon == 0.0) {
        doc[lat_key] = nullptr;
        doc[lon_key] = nullptr;
        return;
    }
    doc[lat_key] = lat;
    doc[lon_key] = lon;
}

String broadcast_get(const ODID_UAS_Data *data) {
    JsonDocument doc;
    doc["type"] = "broadcast";

    // Grouped by source ODID message, in message-type order, even though the
    // shape is flat: the grouping is free here and lets the dash render
    // sections without parsing key names. A message that is not being
    // broadcast contributes no keys at all - the same *Valid flags both
    // broadcast paths are composed from (dri_slot_valid(),
    // odid_message_build_pack()), so absence here means absence on air.
    if (data->BasicIDValid[0]) {
        put_enum(doc, "uas_id_type", UAS_ID_TYPES, ARRAY_COUNT(UAS_ID_TYPES),
                 data->BasicID[0].IDType);
        doc["uas_id"] = data->BasicID[0].UASID;
        put_enum(doc, "ua_type", UA_TYPES, ARRAY_COUNT(UA_TYPES),
                 data->BasicID[0].UAType);
    }

    if (data->LocationValid) {
        // ODID's Location.Status is the *aircraft's* flight state; the
        // sibling message on this socket is the device health status, so the
        // ODID name would collide in a flat payload.
        put_enum(doc, "flight_status", FLIGHT_STATUSES,
                 ARRAY_COUNT(FLIGHT_STATUSES), data->Location.Status);
        put_position(doc, "latitude", "longitude",
                     data->Location.Latitude, data->Location.Longitude);
        put_number(doc, "altitude_geo", data->Location.AltitudeGeo, INV_ALT);
        put_number(doc, "height", data->Location.Height, INV_ALT);
        put_enum(doc, "height_type", HEIGHT_TYPES, ARRAY_COUNT(HEIGHT_TYPES),
                 data->Location.HeightType);
        put_number(doc, "direction", data->Location.Direction, INV_DIR);
        put_number(doc, "speed_horizontal", data->Location.SpeedHorizontal,
                   INV_SPEED_H);
        put_number(doc, "speed_vertical", data->Location.SpeedVertical,
                   INV_SPEED_V);
        // The accuracies stay strings and deliberately do not become null:
        // ODID_*_ACC_UNKNOWN is a declared enum member the device
        // affirmatively transmits ("the GNSS receiver gave us no estimate"),
        // whereas null means a measurement is absent from the broadcast.
        put_enum(doc, "horiz_accuracy", HORIZ_ACCURACIES,
                 ARRAY_COUNT(HORIZ_ACCURACIES), data->Location.HorizAccuracy);
        put_enum(doc, "vert_accuracy", VERT_ACCURACIES,
                 ARRAY_COUNT(VERT_ACCURACIES), data->Location.VertAccuracy);
        put_enum(doc, "speed_accuracy", SPEED_ACCURACIES,
                 ARRAY_COUNT(SPEED_ACCURACIES), data->Location.SpeedAccuracy);
    }

    if (data->SelfIDValid) {
        put_enum(doc, "desc_type", DESC_TYPES, ARRAY_COUNT(DESC_TYPES),
                 data->SelfID.DescType);
        doc["desc"] = data->SelfID.Desc;
    }

    if (data->SystemValid) {
        put_enum(doc, "operator_location_type", OPERATOR_LOCATION_TYPES,
                 ARRAY_COUNT(OPERATOR_LOCATION_TYPES),
                 data->System.OperatorLocationType);
        put_position(doc, "operator_latitude", "operator_longitude",
                     data->System.OperatorLatitude,
                     data->System.OperatorLongitude);
        put_number(doc, "operator_altitude_geo",
                   data->System.OperatorAltitudeGeo, INV_ALT);
    }

    if (data->OperatorIDValid) {
        put_enum(doc, "operator_id_type", OPERATOR_ID_TYPES,
                 ARRAY_COUNT(OPERATOR_ID_TYPES),
                 data->OperatorID.OperatorIdType);
        doc["operator_id"] = data->OperatorID.OperatorId;
    }

    String buf;
    // ~0.6 kB once a second, against a heap ESPAsyncWebServer already
    // fragments: reserve the whole payload instead of growing through a
    // realloc chain. status_get()'s ~200 bytes do not need this.
    buf.reserve(768);
    serializeJson(doc, buf);
    return buf;
}
