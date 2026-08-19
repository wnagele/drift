# GDB-driven end-to-end test of the firmware running in Espressif's QEMU
# (ESP32-C3).
#
# Runs inside gdb: gdb -batch -x test/e2e/firmware.test.py <firmware.elf>
# Normally invoked through the container: make e2e (see test/e2e/docker).
#
# This file is only the driver: the harness (gdb and UART plumbing) lives in
# harness.py, the expected values in expectations.py and the scenarios in
# scenarios/, run in the order listed below. They share one boot: state
# accumulates from one to the next, the way a real MAVLink stream would
# arrive. Breakpoints synchronise instead of serial timeouts, and assertions
# read target memory directly, so internal state that never reaches the
# serial port (decoded ODID location, armed/gps_fix, message counters) can be
# checked. The only wire going into the target is the MAVLink input on UART1,
# which keeps the parser path end-to-end; firmware STDOUT goes to UART0.

import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import gdb

from harness import SCENARIOS, Session

# Narrative order: boot first, then the MAVLink streams arrive, the state
# machine's gates and fallbacks are probed, status is forced, the take-off
# location is set, the broadcast schedule is checked (normal, then with
# unknown-heading telemetry) - followed by the serial log audit of everything
# the firmware printed. scenarios.defaults is NOT part of the narrative: it
# boots a factory-blank config store, so it only runs against the defaults
# image the entrypoint selects it for (see below).
SCENARIO_MODULES = [
    "scenarios.boot",
    "scenarios.defaults",
    "scenarios.telemetry",
    "scenarios.branches",
    "scenarios.status",
    "scenarios.operator",
    "scenarios.broadcast",
    "scenarios.unknown",
    "scenarios.serial",
]

MAVLINK_ADDR = (os.environ.get("DRIFT_MAVLINK_HOST", "127.0.0.1"),
                int(os.environ.get("DRIFT_MAVLINK_PORT", "5555")))
GDB_TARGET = os.environ.get("DRIFT_GDB_TARGET", ":1234")


def load_scenarios():
    """Import the scenario modules, refusing to silently skip a file.

    DRIFT_SCENARIOS (comma-separated module names) selects a subset of the
    canonical list, imported in canonical order. The entrypoint uses it to
    keep scenarios.defaults out of the pre-seeded run and to run it (plus the
    serial audit) in its own two defaults-image boots; without the variable
    every module runs, which is only valid against a seeded image."""
    selected = SCENARIO_MODULES
    env = os.environ.get("DRIFT_SCENARIOS")
    if env:
        wanted = [name.strip() for name in env.split(",") if name.strip()]
        unknown = [name for name in wanted if name not in SCENARIO_MODULES]
        if unknown:
            raise RuntimeError("DRIFT_SCENARIOS names unknown modules: %s"
                               % ", ".join(unknown))
        selected = [module for module in SCENARIO_MODULES if module in wanted]
    listed = {module.rsplit(".", 1)[-1] for module in SCENARIO_MODULES}
    on_disk = {name[:-3] for name in os.listdir(os.path.join(HERE, "scenarios"))
               if name.endswith(".py") and not name.startswith("_")}
    unlisted = on_disk - listed
    if unlisted:
        raise RuntimeError("scenario modules missing from SCENARIO_MODULES: %s"
                           % ", ".join(sorted(unlisted)))
    for module in selected:
        __import__(module)


status = 1
session = Session(MAVLINK_ADDR)
try:
    load_scenarios()
    session.start(GDB_TARGET)
    for title, run in SCENARIOS:
        print("\n%s" % title)
        run(session)
    status = 1 if session.failures else 0
except Exception as error:                    # noqa: BLE001 - report and fail
    print("\nharness error: %s: %s" % (type(error).__name__, error))
    import traceback
    traceback.print_exc()
finally:
    session.close()
    print("\n%d/%d checks passed%s" % (session.passed, session.checks,
          "" if not session.failures else ", failed: " + ", ".join(session.failures)))
    gdb.execute("quit %d" % status)
