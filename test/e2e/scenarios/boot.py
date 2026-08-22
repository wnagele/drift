from expectations import api_fixture
from harness import scenario

CONFIG = api_fixture("config")


@scenario("boot: setup() completes and reaches loop()")
def boot(t):
    t.run_to("loop", finish=False)
    t.check("gps_fix", "false")
    t.check("armed", "false")
    t.check("telemetry_count", 0)
    t.check("gnss_count", 0)
    t.check("odid_state.Location.Status", "ODID_STATUS_UNDECLARED")

    # The NVS partition is pre-seeded with the reference aircraft's config
    # (test/e2e/nvs_seed.py, built from the same shared fixture): the
    # identity must be populated from it at boot.
    t.check("odid_state.BasicID[0].IDType", "ODID_IDTYPE_SERIAL_NUMBER")
    t.check_eq("Basic ID loaded from NVS",
               t.read_string("odid_state.BasicID[0].UASID"), CONFIG["dri"]["ua_id"])
    t.check_eq("Self ID loaded from NVS",
               t.read_string("odid_state.SelfID.Desc"), CONFIG["dri"]["ua_desc"])
    t.check_eq("Operator ID loaded from NVS",
               t.read_string("odid_state.OperatorID.OperatorId"), CONFIG["dri"]["op_id"])
    t.check("odid_state.OperatorID.OperatorIdType", "ODID_OPERATOR_ID")
    t.check("odid_state.SelfID.DescType", "ODID_DESC_TYPE_TEXT")
