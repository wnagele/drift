from expectations import (ODID_HOR_ACC_3_METER, ODID_SPEED_ACC_1_MPS,
                          ODID_VER_ACC_10_METER)
from harness import scenario


@scenario("telemetry: GNSS uncertainty populates the ODID accuracy enums")
def accuracy_from_v2_telemetry(t):
    # GPS_RAW_INT's h_acc/v_acc/vel_acc are MAVLink v2 *extension* fields, so
    # every other fixture here - emitted as v1, like Betaflight's telemetry -
    # truncates them away and the accuracies stay "unknown" (pinned natively
    # by test_mavlink's truncation characterization test). accuracy_v2 is the
    # one v2 stream, standing in for an ArduPilot/PX4 sender, and is the only
    # way to exercise a populated accuracy end to end.
    #
    # The three expected values are deliberately different, so a mis-wiring
    # that swaps two of the fields fails instead of passing.
    t.replay("accuracy_v2")
    t.run_to("dri_update_location")   # GLOBAL_POSITION_INT, the last message

    t.check("odid_state.Location.HorizAccuracy", ODID_HOR_ACC_3_METER)   # 1.5 m
    t.check("odid_state.Location.VertAccuracy", ODID_VER_ACC_10_METER)   # 3.5 m
    t.check("odid_state.Location.SpeedAccuracy", ODID_SPEED_ACC_1_MPS)   # 0.5 m/s

    # The rest of the location still decodes normally from a v2 frame: the
    # pre-extension fields sit at the same offsets in both wire versions.
    t.check("odid_state.Location.Latitude", 473566123 / 1e7, tol=1e-7)
    t.check("odid_state.Location.SpeedHorizontal", 3.5, tol=1e-3)
    t.check("odid_state.Location.SpeedVertical", 2.5, tol=1e-3)
