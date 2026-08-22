import os
import re

from harness import scenario

# Written by QEMU (UART0 -> file, see test/e2e/docker/entrypoint.sh); the
# entrypoint exports DRIFT_SERIAL_LOG for the gdb run.
SERIAL_LOG = os.environ.get("DRIFT_SERIAL_LOG", "/tmp/serial.log")

ERRORS = (
    re.compile(r"\[E\]"),                    # Arduino log: error level
    re.compile(r"^E \(\d+\)", re.MULTILINE),  # IDF log: error level
    re.compile(r"assert failed"),             # ROM/IDF assertions
)


@scenario("serial: the firmware's own output reports no errors")
def serial_no_errors(t):
    # The green baseline used to contain four [E][Preferences] lines (NVS
    # failures) that nobody looked at. Any error the firmware itself logs is
    # a failed test, even if every other check happens to pass.
    with open(SERIAL_LOG) as f:
        log = f.read()
    offenders = [line for line in log.splitlines()
                 if any(p.search(line) for p in ERRORS)]
    t.check_eq("no error lines in the UART0 log", offenders, [])
