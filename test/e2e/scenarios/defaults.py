import os

from harness import scenario

# Runs against the *defaults* flash image (factory-blank NVS partition, see
# test/e2e/docker/entrypoint.sh), selected via DRIFT_SCENARIOS: these checks
# are invalid against the main suite's pre-seeded image, so the two boots run
# separately.
#
# Booting blank config storage is the only path that exercises config_init()'s
# writes - the seeded image needs none, which is how the writes silently
# failing under QEMU went unnoticed. Power cycle 1 asserts the defaults were
# written and readable (the DRIFT-<efuse> SSID via getDefaultSSID(), empty
# everything else); power cycle 2 boots the same image file again and asserts
# the same values come back, i.e. the writes actually committed to flash -
# the persistence property the NVS-under-QEMU breakage violated.

LABEL = os.environ.get("DRIFT_RUN_LABEL", "factory boot")


@scenario("defaults: config_init writes the factory config (%s)" % LABEL)
def defaults(t):
    t.run_to("loop", finish=False)

    # The stored SSID is the one getDefaultSSID() derives from the eFuse MAC:
    # written by config_init(), read back through the storage seam. Both
    # sides are evaluated on the target and compared by content - a dropped
    # write reads back as "" and fails this.
    stored = t.read_arduino_string("config_wifi_ssid()")
    default = t.read_arduino_string("getDefaultSSID()")
    t.check_eq("stored SSID is the eFuse-derived default", stored, default)

    # The default SSID shape itself (utils.h: DRIFT_%02X%02X of the two upper
    # MAC bytes; the formatting is pinned natively by test_utils).
    t.check_eq("default SSID shape", (stored[:6], len(stored)), ("DRIFT_", 10))

    # Every other key defaults to empty. The accessors setup() calls survive
    # as symbols in this build; wifi_password's empty default is pinned
    # natively by test_config (test_init_creates_defaults).
    t.check_eq("dri_ua_id defaults to empty",
               t.read_arduino_string("config_dri_ua_id()"), "")
    t.check_eq("dri_ua_desc defaults to empty",
               t.read_arduino_string("config_dri_ua_desc()"), "")
    t.check_eq("dri_op_id defaults to empty",
               t.read_arduino_string("config_dri_op_id()"), "")

    # And empty is what the identity population saw at boot: no ID slots
    # configured.
    t.check_eq("no Basic ID on a factory boot",
               t.read_string("odid_state.BasicID[0].UASID"), "")
    t.check_eq("no Self ID on a factory boot",
               t.read_string("odid_state.SelfID.Desc"), "")
    t.check_eq("no Operator ID on a factory boot",
               t.read_string("odid_state.OperatorID.OperatorId"), "")
