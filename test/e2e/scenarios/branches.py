from expectations import MAV_STATE_TO_ODID
from harness import scenario

# Position from the telemetry scenario's disarmed_with_fix replay: the
# gated fixtures below carry different coordinates (47.0/8.0), so a broken
# gate visibly overwrites it.
LAST_LAT = 473566123 / 1e7


@scenario("branches: position and origin without a fix are ignored")
def position_ignored_without_fix(t):
    # The no_fix_position stream delivers GLOBAL_POSITION_INT and
    # GPS_GLOBAL_ORIGIN while gps_fix is false (no prior fix). Both must be
    # dropped: the location keeps its previous value, the operator/take-off
    # location stays at its init default, and no GNSS reception is counted.
    t.replay("no_fix_position")
    t.run_to("status_telemetry_rcvd")         # HEARTBEAT, the last message
    t.check("gps_fix", "false")
    t.check("armed", "true")
    t.check("odid_state.Location.Latitude", LAST_LAT, tol=1e-7)   # unchanged
    t.check("odid_state.System.OperatorLatitude", 0, tol=1e-7)    # never set
    t.check("gnss_count", 1)                                       # unchanged


@scenario("branches: a 2D fix does not unlock the position")
def two_d_fix_does_not_count(t):
    # fix_type 2 is below the 3D threshold: the position must still be
    # ignored (a '>=' vs '>' regression on the threshold would accept it).
    t.replay("fix_2d")
    t.run_to("status_telemetry_rcvd")
    t.check("gps_fix", "false")
    t.check("odid_state.Location.Latitude", LAST_LAT, tol=1e-7)   # unchanged
    t.check("gnss_count", 1)                                       # unchanged


@scenario("branches: failsafe states map to ODID_STATUS_EMERGENCY")
def emergency_status(t):
    # CRITICAL first ...
    t.replay("emergency")
    t.run_to("status_telemetry_rcvd")
    t.check("odid_state.Location.Status", MAV_STATE_TO_ODID[5])
    t.check("armed", "true")
    # ... then FLIGHT_TERMINATION: same emergency mapping, still armed.
    t.run_to("status_telemetry_rcvd")
    t.check("odid_state.Location.Status", MAV_STATE_TO_ODID[8])
    t.check("armed", "true")


@scenario("branches: an unmapped status falls back to UNDECLARED and disarmed")
def unmapped_status_is_undeclared(t):
    # POWEROFF is not one of the mapped states: the default branch reports
    # ODID_STATUS_UNDECLARED and clears armed.
    t.replay("undeclared")
    t.run_to("status_telemetry_rcvd")
    t.check("odid_state.Location.Status", "ODID_STATUS_UNDECLARED")
    t.check("armed", "false")
