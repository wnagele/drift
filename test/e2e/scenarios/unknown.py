from expectations import INV_ALT, INV_DIR, odid_fixture
from harness import scenario
from scenarios.broadcast import CYCLE_LENGTH, cycle_of, full_cycle


@scenario("telemetry: unknown heading and out-of-range altitudes are reported as unknown")
def unknown_telemetry(t):
    # MAVLink defines hdg = UINT16_MAX as "heading unknown"; altitudes near
    # INT32_MAX (mm) have no ODID representation. The range guards in
    # main.cpp must map these to the ODID unknown sentinels (INV_DIR /
    # INV_ALT) - before them, hdg became 655.35 deg, the encoder rejected
    # the location data, and every location slot broadcast an all-zero
    # message while armed (the hdg-centidegrees bug's failure mode, still
    # live at the time of the review).
    t.replay("armed_unknown")
    t.run_to("dri_update_location")

    t.check("odid_state.Location.Direction", INV_DIR)
    t.check("odid_state.Location.Height", INV_ALT)
    t.check("odid_state.Location.AltitudeGeo", INV_ALT)
    t.check("odid_state.Location.Latitude", 473566123 / 1e7, tol=1e-7)  # position itself survives

    # ... and the broadcast location slots carry a real, valid message with
    # the unknown sentinels - not zeros, and not a skipped schedule.
    sends = t.ble_sends(full_cycle)
    cycle = cycle_of(sends)
    if cycle is None:
        t.check_eq("a full 25-slot schedule captured", len(sends), CYCLE_LENGTH)
        return
    t.check_eq("every location slot matches the unknown-sentinels reference encoding",
               [record[1] for record in cycle[4:CYCLE_LENGTH]],
               [odid_fixture("location_unknown")] * (CYCLE_LENGTH - 4))
