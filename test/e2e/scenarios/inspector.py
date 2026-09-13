import json

from expectations import api_fixture
from harness import scenario


@scenario("inspector: the broadcast payload reports what the device is sending")
def broadcast_payload(t):
    # The dash's broadcast inspector panel: broadcast_get() reads the one
    # ODID_UAS_Data the transports are composed from and hands the dash
    # display-ready strings, so this is where the enum vocabulary and the
    # sentinel translation get pinned against a real device rather than
    # against a unit-test struct.
    #
    # Runs on the shared narrative state: the reference aircraft configured
    # from NVS, armed_fix telemetry replayed by the broadcast scenario and
    # the take-off origin set by the operator scenario.
    payload = json.loads(t.read_arduino_string("broadcast_get(&odid_state)"))
    config = api_fixture("config")["dri"]

    t.check_eq("payload type", payload["type"], "broadcast")

    # The identity comes from NVS, through dri_populate_identity().
    t.check_eq("uas_id is the configured serial",
               payload["uas_id"], config["ua_id"])
    t.check_eq("operator_id is the configured registration number",
               payload["operator_id"], config["op_id"])
    t.check_eq("the verification code is nowhere in the payload",
               config["op_secret"] in json.dumps(payload), False)
    # The device *does* have a Self-ID configured, unlike the shared payload
    # fixture (which drops it to exercise the omission rule).
    t.check_eq("desc is the configured Self-ID",
               payload["desc"], config["ua_desc"])

    # Every enum arrives as the word the dash prints, so the dash holds no
    # ordinal table. These are the API.
    t.check_eq("enum vocabulary",
               {key: payload[key] for key in (
                   "uas_id_type", "ua_type", "flight_status", "height_type",
                   "desc_type", "operator_location_type", "operator_id_type")},
               {"uas_id_type": "Serial Number", "ua_type": "None",
                "flight_status": "Airborne", "height_type": "Above Take-off",
                "desc_type": "Text", "operator_location_type": "Take-off",
                "operator_id_type": "Operator ID"})

    # The reference capture is MAVLink v1, which truncates the GPS_RAW_INT
    # uncertainty extensions away: "Unknown" is a value the device
    # transmits, so it stays a word rather than becoming null.
    t.check_eq("accuracies read unknown on a MAVLink v1 sender",
               [payload["horiz_accuracy"], payload["vert_accuracy"],
                payload["speed_accuracy"]], ["Unknown"] * 3)

    # The measurements, straight off the replayed telemetry.
    t.check_eq("location",
               [payload["latitude"], payload["longitude"],
                payload["altitude_geo"], payload["height"],
                payload["direction"], payload["speed_horizontal"],
                payload["speed_vertical"]],
               [47.3566123, 8.5321456, 500.5, 30.5, 90, 3.5, 2.5])
    t.check_eq("take-off point",
               [payload["operator_latitude"], payload["operator_longitude"],
                payload["operator_altitude_geo"]],
               [47.3566, 8.5321, 500])

    # And the whole thing agrees with the shared fixture the dash tests
    # assert against - every key of it, with the Self-ID the fixture omits
    # being the only difference.
    fixture = api_fixture("broadcast")
    t.check_eq("payload matches the shared fixture",
               {key: payload.get(key) for key in fixture}, fixture)
    t.check_eq("the only extra keys are the Self-ID the fixture drops",
               sorted(set(payload) - set(fixture)), ["desc", "desc_type"])
