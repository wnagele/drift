#include <unity.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <string.h>

#include "broadcast.h"
#include "dri.h"
#include "../support/fixtures.h"

// The reference aircraft, same values as test_dri and
// test/tools/gen_odid_fixtures.cpp - with one deliberate difference: no
// Self-ID is configured here, because that is what exercises the payload's
// omission rule (a serializer that never omits anything would otherwise
// pass). The shared fixture is the same aircraft, so the two agree.
static const char UA_ID[] = "1DRIFT000TEST00001";
static const char OP_ID[] = "AUTdrift0test01h";
static const char UA_DESC[] = "DRIFT test fixture";
static const double LAT = 47.3566123;
static const double LON = 8.5321456;
static const double ALT_GEO = 500.5;
static const double HEIGHT = 30.5;
static const float DIRECTION = 90.0;
static const float SPEED_HORIZONTAL = 3.5;
static const float SPEED_VERTICAL = 2.5;
static const float TIMESTAMP = 2096.7;
static const double OPERATOR_LAT = 47.3566000;
static const double OPERATOR_LON = 8.5321000;
static const double OPERATOR_ALT_GEO = 500.0;

static ODID_UAS_Data data;

void setUp() {
    // dri_init() is the firmware's own starting point: the ODID "unknown"
    // sentinels everywhere, LocationValid forced on, and the explicit
    // INV_TIMESTAMP the vendored initialiser leaves out.
    dri_init(&data, 0);
}

// The reference aircraft's Location/Vector update. The accuracies are
// "unknown" because the reference capture is MAVLink v1, which truncates the
// GPS_RAW_INT uncertainty extensions away - the real Betaflight case.
static DriLocation reference_location() {
    DriLocation location = {};
    location.latitude = LAT;
    location.longitude = LON;
    location.altitude_geo = ALT_GEO;
    location.height = HEIGHT;
    location.direction = DIRECTION;
    location.speed_horizontal = SPEED_HORIZONTAL;
    location.speed_vertical = SPEED_VERTICAL;
    location.timestamp = TIMESTAMP;
    location.horizontal_accuracy = ODID_HOR_ACC_UNKNOWN;
    location.vertical_accuracy = ODID_VER_ACC_UNKNOWN;
    location.speed_accuracy = ODID_SPEED_ACC_UNKNOWN;
    return location;
}

// A configured aircraft receiving good telemetry, built through the same
// calls main.cpp makes. `ua_desc` is a parameter so the Self-ID can be left
// unconfigured (the fixture case) or present.
static void build_reference_data(const char *ua_desc) {
    dri_populate_identity(&data, UA_ID, OP_ID, ua_desc);
    dri_update_status(&data, ODID_STATUS_AIRBORNE);
    DriLocation location = reference_location();
    dri_update_location(&data, &location);
    dri_update_operator(&data, OPERATOR_LAT, OPERATOR_LON, OPERATOR_ALT_GEO);
}

static JsonDocument get_broadcast() {
    JsonDocument doc;
    TEST_ASSERT_FALSE(deserializeJson(doc, broadcast_get(&data)));
    return doc;
}

// Present-or-absent has to be distinguishable from present-and-null, which
// is the whole point of the payload's two negative states - and
// `doc[key].isNull()` cannot tell them apart, so look the key up by name.
static bool member(JsonObject obj, const char *key, JsonVariant *out) {
    for (JsonPair kv : obj) {
        if (strcmp(kv.key().c_str(), key) == 0) {
            if (out)
                *out = kv.value();
            return true;
        }
    }
    return false;
}

static void assert_absent(JsonDocument &doc, const char *key) {
    TEST_ASSERT_FALSE_MESSAGE(member(doc.as<JsonObject>(), key, nullptr), key);
}

static void assert_null(JsonDocument &doc, const char *key) {
    JsonVariant value;
    TEST_ASSERT_TRUE_MESSAGE(member(doc.as<JsonObject>(), key, &value), key);
    TEST_ASSERT_TRUE_MESSAGE(value.isNull(), key);
}

static void assert_string(JsonDocument &doc, const char *key,
                          const char *expected) {
    JsonVariant value;
    TEST_ASSERT_TRUE_MESSAGE(member(doc.as<JsonObject>(), key, &value), key);
    TEST_ASSERT_EQUAL_STRING_MESSAGE(expected, value.as<const char *>(), key);
}

// --- The shared contract --------------------------------------------------

void test_matches_shared_fixture() {
    // Contract: test/fixtures/api/broadcast.json, asserted by the dash tests
    // too. Compared key by key *and* by key count, so neither side can grow
    // a field the other does not know about.
    build_reference_data("");
    JsonDocument fixture;
    std::string raw = fixture_read("api/broadcast.json");
    TEST_ASSERT_FALSE(raw.empty());
    TEST_ASSERT_FALSE(deserializeJson(fixture, raw));
    JsonDocument doc = get_broadcast();

    JsonObject want = fixture.as<JsonObject>();
    TEST_ASSERT_EQUAL_size_t(want.size(), doc.as<JsonObject>().size());
    for (JsonPair kv : want) {
        const char *key = kv.key().c_str();
        JsonVariant got;
        TEST_ASSERT_TRUE_MESSAGE(member(doc.as<JsonObject>(), key, &got), key);
        if (kv.value().is<const char *>())
            TEST_ASSERT_EQUAL_STRING_MESSAGE(kv.value().as<const char *>(),
                                             got.as<const char *>(), key);
        else
            TEST_ASSERT_EQUAL_DOUBLE_MESSAGE(kv.value().as<double>(),
                                             got.as<double>(), key);
    }
}

void test_fixture_omits_the_self_id_deliberately() {
    // The fixture pins the absent case; this pins the other direction, so
    // the omission cannot be a serializer that simply never emits a Self-ID.
    build_reference_data(UA_DESC);
    JsonDocument doc = get_broadcast();
    assert_string(doc, "desc_type", "Text");
    assert_string(doc, "desc", UA_DESC);
}

// --- The fresh device ------------------------------------------------------

void test_fresh_device_omits_every_unconfigured_message() {
    // Nothing configured and no telemetry yet: four of the five messages are
    // not broadcast at all (dri_slot_valid() skips their slots and the pack
    // builder leaves them out), so their fields are absent rather than
    // empty. Four missing groups is how a user learns their identity config
    // never took effect.
    JsonDocument doc = get_broadcast();
    static const char *absent[] = {
        "uas_id_type", "uas_id", "ua_type",                 // Basic ID
        "desc_type", "desc",                                // Self-ID
        "operator_location_type", "operator_latitude",      // System
        "operator_longitude", "operator_altitude_geo",
        "operator_id_type", "operator_id",                  // Operator ID
    };
    for (const char *key : absent)
        assert_absent(doc, key);
    // Location is the one group always present: dri_init() forces
    // LocationValid, so the location *is* on air from power-on - carrying
    // ODID's "no value" everywhere.
    assert_string(doc, "flight_status", "Undeclared");
    assert_string(doc, "height_type", "Above Take-off");
}

void test_fresh_device_reports_no_value_as_null() {
    // The sentinel translation the shared fixture cannot cover (it holds no
    // null): ODID reports missing data as in-band values, and a receiver
    // renders them as blanks. Here is where "no GNSS speed estimate" becomes
    // legible.
    JsonDocument doc = get_broadcast();
    static const char *null_keys[] = {
        "latitude", "longitude", "altitude_geo", "height", "direction",
        "speed_horizontal", "speed_vertical",
    };
    for (const char *key : null_keys)
        assert_null(doc, key);
    // ODID_*_ACC_UNKNOWN is a declared value the device affirmatively
    // transmits, not an absent measurement, so it stays a string.
    assert_string(doc, "horiz_accuracy", "Unknown");
    assert_string(doc, "vert_accuracy", "Unknown");
    assert_string(doc, "speed_accuracy", "Unknown");
}

void test_position_no_value_is_a_pair_test() {
    // ODID's unknown for a position is *both* coordinates at zero, so a
    // single populated coordinate is a real position - the standard's rule,
    // not a rounding accident.
    build_reference_data("");
    data.Location.Latitude = 0.0;
    data.System.OperatorLatitude = 0.0;
    JsonDocument doc = get_broadcast();
    JsonVariant latitude, longitude;
    TEST_ASSERT_TRUE(member(doc.as<JsonObject>(), "latitude", &latitude));
    TEST_ASSERT_TRUE(member(doc.as<JsonObject>(), "longitude", &longitude));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, latitude.as<double>());
    // Read back as float, not double, on purpose: ArduinoJson's *parser*
    // packs a decimal whose mantissa fits float's into a float variant
    // (parseNumber.hpp), so re-reading the payload here is lossier than the
    // payload itself. The payload text is pinned separately, by
    // test_latitude_keeps_full_resolution.
    TEST_ASSERT_EQUAL_FLOAT((float)LON, longitude.as<float>());
    TEST_ASSERT_EQUAL_FLOAT((float)OPERATOR_LON,
                            doc["operator_longitude"].as<float>());
}

void test_latitude_keeps_full_resolution() {
    // ODID encodes coordinates in 1e-7 degrees (~11 mm), and the payload
    // must not quietly round that away - a truncated coordinate would look
    // like a real position several metres from the aircraft. ArduinoJson
    // writes doubles with up to nine decimal places; pin it rather than
    // trust it.
    build_reference_data("");
    data.Location.Latitude = 47.1234567;
    data.Location.Longitude = -8.7654321;
    String payload = broadcast_get(&data);
    TEST_ASSERT_NOT_NULL(strstr(payload.c_str(), "47.1234567"));
    TEST_ASSERT_NOT_NULL(strstr(payload.c_str(), "-8.7654321"));
}

// --- Reserved ordinals ----------------------------------------------------

// Every one of these enums has reserved gaps in the standard. No device
// state can produce one today (every value DRIFT writes is hardcoded or
// range-checked by the vendored encoders), so the fallback needs a test
// rather than a hope: a blank would read as a serializer bug, while a
// receiver would render something.
void test_reserved_ordinals_serialize_as_reserved() {
    build_reference_data(UA_DESC);
    data.BasicID[0].IDType = (ODID_idtype_t)5;
    data.BasicID[0].UAType = (ODID_uatype_t)16;
    data.Location.Status = (ODID_status_t)5;
    data.Location.HeightType = (ODID_Height_reference_t)2;
    data.Location.HorizAccuracy = (ODID_Horizontal_accuracy_t)13;
    data.Location.VertAccuracy = (ODID_Vertical_accuracy_t)7;
    data.Location.SpeedAccuracy = (ODID_Speed_accuracy_t)5;
    data.SelfID.DescType = (ODID_desctype_t)3;
    data.System.OperatorLocationType = (ODID_operator_location_type_t)3;
    data.OperatorID.OperatorIdType = (ODID_operatorIdType_t)1;
    JsonDocument doc = get_broadcast();
    assert_string(doc, "uas_id_type", "Reserved (5)");
    assert_string(doc, "ua_type", "Reserved (16)");
    assert_string(doc, "flight_status", "Reserved (5)");
    assert_string(doc, "height_type", "Reserved (2)");
    assert_string(doc, "horiz_accuracy", "Reserved (13)");
    assert_string(doc, "vert_accuracy", "Reserved (7)");
    assert_string(doc, "speed_accuracy", "Reserved (5)");
    assert_string(doc, "desc_type", "Reserved (3)");
    assert_string(doc, "operator_location_type", "Reserved (3)");
    assert_string(doc, "operator_id_type", "Reserved (1)");
}

void test_last_ordinal_of_each_enum_is_named() {
    // The off-by-one neighbour of the test above: the highest *defined*
    // ordinal must still read as a word, or the bounds check is one short.
    build_reference_data(UA_DESC);
    data.BasicID[0].IDType = ODID_IDTYPE_SPECIFIC_SESSION_ID;
    data.BasicID[0].UAType = ODID_UATYPE_OTHER;
    data.Location.Status = ODID_STATUS_REMOTE_ID_SYSTEM_FAILURE;
    data.Location.HeightType = ODID_HEIGHT_REF_OVER_GROUND;
    data.Location.HorizAccuracy = ODID_HOR_ACC_1_METER;
    data.Location.VertAccuracy = ODID_VER_ACC_1_METER;
    data.Location.SpeedAccuracy = ODID_SPEED_ACC_0_3_METERS_PER_SECOND;
    data.SelfID.DescType = ODID_DESC_TYPE_EXTENDED_STATUS;
    data.System.OperatorLocationType = ODID_OPERATOR_LOCATION_TYPE_FIXED;
    JsonDocument doc = get_broadcast();
    assert_string(doc, "uas_id_type", "Specific Session ID");
    assert_string(doc, "ua_type", "Other");
    // Unreachable on a real device: the 5 s input-staleness rule that sets
    // it is the unbuilt FC interlock. Covered here so the vocabulary is
    // complete when it lands.
    assert_string(doc, "flight_status", "Remote ID System Failure");
    assert_string(doc, "height_type", "Above Ground");
    assert_string(doc, "horiz_accuracy", "< 1 m");
    assert_string(doc, "vert_accuracy", "< 1 m");
    assert_string(doc, "speed_accuracy", "< 0.3 m/s");
    assert_string(doc, "desc_type", "Extended Status");
    assert_string(doc, "operator_location_type", "Fixed");
}

void test_type_identifies_the_message() {
    // The dash switches on it: a second message type on the same socket as
    // the status message.
    JsonDocument doc = get_broadcast();
    assert_string(doc, "type", "broadcast");
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_matches_shared_fixture);
    RUN_TEST(test_fixture_omits_the_self_id_deliberately);
    RUN_TEST(test_fresh_device_omits_every_unconfigured_message);
    RUN_TEST(test_fresh_device_reports_no_value_as_null);
    RUN_TEST(test_position_no_value_is_a_pair_test);
    RUN_TEST(test_latitude_keeps_full_resolution);
    RUN_TEST(test_reserved_ordinals_serialize_as_reserved);
    RUN_TEST(test_last_ordinal_of_each_enum_is_named);
    RUN_TEST(test_type_identifies_the_message);
    return UNITY_END();
}
