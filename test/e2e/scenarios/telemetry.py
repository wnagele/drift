from expectations import INV_ALT, INV_DIR, MAV_STATE_TO_ODID
from harness import scenario


@scenario("telemetry: disarmed with a 3D fix")
def disarmed_with_fix(t):
    t.replay("disarmed_fix")
    t.run_to("status_telemetry_rcvd")         # HEARTBEAT, the last message in the fixture
    t.check("gps_fix", "true")
    t.check("armed", "false")
    t.check("odid_state.Location.Status", MAV_STATE_TO_ODID[3])     # MAV_STATE_STANDBY
    t.check("odid_state.Location.Latitude", 473566123 / 1e7, tol=1e-7)   # 47.3566123 deg
    t.check("odid_state.Location.Longitude", 85321456 / 1e7, tol=1e-7)   # 8.5321456 deg
    t.check("odid_state.Location.AltitudeGeo", 500500 / 1e3, tol=1e-3)   # 500.5 m
    t.check("odid_state.Location.Height", INV_ALT)      # withheld while disarmed
    t.check("odid_state.Location.Direction", INV_DIR)   # withheld while disarmed
    t.check("telemetry_count", 1)
    t.check("gnss_count", 1)


@scenario("telemetry: armed without a fix")
def armed_without_fix(t):
    t.replay("armed_no_fix")
    t.run_to("status_telemetry_rcvd")
    t.check("gps_fix", "false")
    t.check("armed", "true")
    t.check("odid_state.Location.Status", MAV_STATE_TO_ODID[4])     # MAV_STATE_ACTIVE
