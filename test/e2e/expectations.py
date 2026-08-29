# Expected constants for the e2e scenarios. The MAVLink captures come from
# the generated fixtures (test/fixtures/mavlink/*.bin, written by
# `make fixtures`); everything else is owned by the tests. The literals below
# mirror lib/libopendroneid/opendroneid.h.

import json
import os

WORKSPACE = os.environ.get("DRIFT_WORKSPACE", "/work")
MAVLINK_FIXTURES = os.path.join(WORKSPACE, "test/fixtures/mavlink")
ODID_FIXTURES = os.path.join(WORKSPACE, "test/fixtures/odid")
ODID_MESSAGE_SIZE = 25   # opendroneid.h


def mavlink_fixture(name):
    """Raw bytes of a generated MAVLink capture (<name>.bin)."""
    with open(os.path.join(MAVLINK_FIXTURES, name + ".bin"), "rb") as f:
        return f.read()


def odid_fixture(name):
    """Raw bytes of a generated ODID reference encoding (<name>.bin)."""
    with open(os.path.join(ODID_FIXTURES, name + ".bin"), "rb") as f:
        return f.read()


def api_fixture(name):
    """Parsed shared API fixture (test/fixtures/api/<name>.json), the same
    contract the firmware and dash unit tests assert against."""
    with open(os.path.join(WORKSPACE, "test/fixtures/api", name + ".json")) as f:
        return json.load(f)


INV_ALT = -1000        # ODID invalid altitude (MIN_ALT)
INV_DIR = 361          # ODID invalid direction
ODID_VERSION = 2       # ODID_PROTOCOL_VERSION

# main.cpp maps MAV_STATE onto the ODID operational status. Failsafe states
# (critical, emergency, flight termination = 8) map to EMERGENCY; anything
# else - e.g. poweroff (7) - falls through to UNDECLARED and disarmed.
MAV_STATE_TO_ODID = {3: "ODID_STATUS_GROUND", 4: "ODID_STATUS_AIRBORNE",
                     5: "ODID_STATUS_EMERGENCY", 6: "ODID_STATUS_EMERGENCY",
                     8: "ODID_STATUS_EMERGENCY"}

# dri_slot_type() maps the broadcast schedule counter onto a message type; the
# first encoded byte is (message type << 4) | protocol version. The 10-slot
# cycle allocates the F3411 message rates: Location 2.5 Hz, Basic ID and
# System 2 Hz (the FAA/Japan 1 Hz requirement), Self-ID and Operator ID 1 Hz
# (against the 3 s baseline).
SCHEDULE_TO_MESSAGE_TYPE = {1: "ODID_MESSAGETYPE_BASIC_ID",
                            2: "ODID_MESSAGETYPE_LOCATION",
                            3: "ODID_MESSAGETYPE_SYSTEM",
                            4: "ODID_MESSAGETYPE_LOCATION",
                            5: "ODID_MESSAGETYPE_SELF_ID",
                            6: "ODID_MESSAGETYPE_LOCATION",
                            7: "ODID_MESSAGETYPE_BASIC_ID",
                            8: "ODID_MESSAGETYPE_LOCATION",
                            9: "ODID_MESSAGETYPE_SYSTEM",
                            10: "ODID_MESSAGETYPE_OPERATOR_ID"}
