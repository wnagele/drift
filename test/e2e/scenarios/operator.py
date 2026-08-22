from harness import scenario


@scenario("operator: GPS_GLOBAL_ORIGIN sets the take-off location")
def origin(t):
    t.replay("origin_set")
    t.run_to("dri_update_operator")
    t.check("odid_state.System.OperatorLocationType",
            "ODID_OPERATOR_LOCATION_TYPE_TAKEOFF")
    t.check("odid_state.System.OperatorLatitude", 473566000 / 1e7, tol=1e-7)  # 47.3566 deg
    t.check("odid_state.System.OperatorLongitude", 85321000 / 1e7, tol=1e-7)  # 8.5321 deg
    t.check("odid_state.System.OperatorAltitudeGeo", 500000 / 1e3, tol=1e-3)  # 500 m
