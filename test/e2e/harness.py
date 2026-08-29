# In-gdb harness for the e2e suite: a Session that wraps the gdb Python API
# and the emulated UART, plus the `scenario` decorator the scenario modules
# register themselves with. Loaded by firmware.test.py, which puts this
# directory on sys.path.

import socket
import time

import gdb

from expectations import ODID_MESSAGE_SIZE, mavlink_fixture


class Session:
    """The firmware under gdb, synchronised with breakpoints, fed over UART1."""

    def __init__(self, serial_addr):
        self.serial_addr = serial_addr
        self.serial = None
        self.checks = 0
        self.passed = 0
        self.failures = []

    def start(self, gdb_target):
        """Attach to the target and the emulated UART."""
        self.x("set pagination off")
        self.x("set confirm off")
        try:
            self.x("set print frame-info short-location")  # one line per sync point
        except gdb.error:
            pass
        self.x("target remote %s" % gdb_target)
        self.serial = self._connect_serial()

    def close(self):
        if self.serial:
            self.serial.close()
            self.serial = None

    def x(self, command):
        """Run a gdb command, keeping its output out of the test log."""
        return gdb.execute(command, to_string=True)

    def eval(self, expression):
        """Value of a target expression as an int (enum names, constants)."""
        return int(gdb.parse_and_eval(expression))

    def read_string(self, expression):
        """Value of a target char-array expression as a Python string."""
        return gdb.parse_and_eval(expression).string()

    def read_arduino_string(self, expression):
        """Contents of an Arduino String evaluated on the target.

        gdb cannot call the inline String methods (operator==, c_str, ...),
        so the returned object's own members are read instead - by name,
        resolved from the ELF's type info (WString.h: SSO inline buffer vs
        pointer, packed behind a flag bit). The contents must be extracted
        before the next inferior call: the dummy frame holding the return
        object is reused then."""
        value = gdb.parse_and_eval(expression)
        if int(value["sso"]["isSSO"]):
            return value["sso"]["buff"].string()
        return value["ptr"]["buff"].string()

    def call(self, function):
        """Invoke a void function on the target."""
        self.x("call (void) %s" % function)

    def run_to(self, function, finish=True, timeout=30):
        """Continue until `function` is entered, optionally letting it return.

        A missed breakpoint raises RuntimeError (naming it) instead of
        hanging until the global run timeout with no diagnostics."""
        deadline = time.time() + timeout
        heartbeat = _Heartbeat(deadline)
        try:
            self.x("tbreak %s" % function)
            self.x("continue")
            stopped_at = None
            try:
                stopped_at = gdb.selected_frame().name()
            except gdb.error:
                pass
            if stopped_at != function:
                raise RuntimeError("run_to(%s): breakpoint not reached within "
                                   "%ds (stopped at %s)" % (function, timeout,
                                                            stopped_at))
            if finish:
                self.x("finish")
        finally:
            heartbeat.delete()

    def replay(self, name):
        """Feed a generated MAVLink fixture into UART1."""
        payload = mavlink_fixture(name)
        print("\n  -> UART1: %s (%d bytes)" % (name, len(payload)))
        self.serial.sendall(payload)

    def check(self, expression, expected, tol=None):
        """Assert a target expression; `expected` may be a target expression too."""
        self.checks += 1
        actual = gdb.parse_and_eval(expression)
        try:
            wanted = gdb.parse_and_eval(str(expected))
            ok = (abs(float(actual) - float(wanted)) <= tol if tol is not None
                  else str(actual) == str(wanted))
            expected_text = ("%s (%s)" % (expected, wanted)
                             if str(expected) != str(wanted) else str(wanted))
        except gdb.error:                     # not a target expression, compare text
            ok = str(actual) == str(expected)
            expected_text = str(expected)
        if ok:
            self.passed += 1
            print("  ok   %s = %s" % (expression, actual))
        else:
            self.failures.append(expression)
            print("  FAIL %s = %s, expected %s" % (expression, actual, expected_text))

    def check_eq(self, label, actual, expected):
        """Assert two python values (where target expressions can't go)."""
        self.checks += 1
        if actual == expected:
            self.passed += 1
            print("  ok   %s" % label)
        else:
            self.failures.append(label)
            print("  FAIL %s" % label)
            print("       expected: %s" % (expected,))
            print("       actual:   %s" % (actual,))

    def collect_calls(self, function, read, until, timeout=60):
        """Run the target, recording read() at every call of `function`
        (hits auto-continue), until until(records) is satisfied. Returns the
        records; raises RuntimeError if the condition is not met within
        `timeout` seconds of target run time."""
        deadline = time.time() + timeout
        heartbeat = _Heartbeat(deadline)
        collector = _Collector(function, read, until)
        try:
            self.x("continue")
        finally:
            collector.delete()
            heartbeat.delete()
        if collector.error:
            raise RuntimeError("collect_calls(%s): %s" % (function, collector.error))
        if collector.capped:
            raise RuntimeError("collect_calls(%s): more than %d records before "
                               "the condition was met" % (function, len(collector.records)))
        if heartbeat.timed_out:
            raise RuntimeError("collect_calls(%s): condition not met within %ds "
                               "(%d records)" % (function, timeout, len(collector.records)))
        if not collector.satisfied:
            raise RuntimeError("collect_calls(%s): target stopped unexpectedly "
                               "after %d records" % (function, len(collector.records)))
        return collector.records

    def ble_sends(self, until):
        """Record every ble_send() call as (msg_counter, message bytes,
        millis()).

        The e2e build's no-op stub carries no parameter debug info (empty
        body at -Os), but at function entry the RISC-V calling convention
        still holds the two arguments in a0/a1 - and since ble.cpp is a
        separate translation unit, the empty stub survives as a real,
        breakable function. millis() is read on the target at the same
        moment, so the broadcast cadence can be asserted."""
        def read():
            counter = int(gdb.parse_and_eval("$a0")) & 0xFF
            message = int(gdb.parse_and_eval("$a1"))
            return (counter,
                    bytes(gdb.selected_inferior().read_memory(message, ODID_MESSAGE_SIZE)),
                    int(gdb.parse_and_eval("millis()")))
        return self.collect_calls("ble_send", read, until)

    def ble_pack_sends(self, until):
        """Record every ble_send_pack() call as (msg_counter, pack bytes,
        pack length, millis()), same a0/a1 mechanism as ble_sends() with the
        length from a2."""
        def read():
            counter = int(gdb.parse_and_eval("$a0")) & 0xFF
            pack = int(gdb.parse_and_eval("$a1"))
            length = int(gdb.parse_and_eval("$a2"))
            return (counter,
                    bytes(gdb.selected_inferior().read_memory(pack, length)),
                    length,
                    int(gdb.parse_and_eval("millis()")))
        return self.collect_calls("ble_send_pack", read, until)

    def wifi_beacon_sends(self, until):
        """Record every wifi_beacon_send_pack() call as (msg_counter, pack
        bytes, pack length, millis()), same a0/a1/a2 mechanism as
        ble_pack_sends() at the Wi-Fi Beacon stub's entry."""
        def read():
            counter = int(gdb.parse_and_eval("$a0")) & 0xFF
            pack = int(gdb.parse_and_eval("$a1"))
            length = int(gdb.parse_and_eval("$a2"))
            return (counter,
                    bytes(gdb.selected_inferior().read_memory(pack, length)),
                    length,
                    int(gdb.parse_and_eval("millis()")))
        return self.collect_calls("wifi_beacon_send_pack", read, until)

    def _connect_serial(self):
        deadline = time.time() + 30
        while time.time() < deadline:
            try:
                return socket.create_connection(self.serial_addr, timeout=10)
            except OSError:
                time.sleep(0.2)
        raise RuntimeError("could not connect to the emulated UART on %s:%d"
                           % self.serial_addr)


class _Collector(gdb.Breakpoint):
    """Auto-continuing breakpoint that records read() at every hit and stops
    for real once until(records) says so (or on error / 200 records)."""

    def __init__(self, spec, read, until):
        super().__init__(spec)
        self.silent = True
        self.records = []
        self.error = None
        self.capped = False
        self.satisfied = False
        self._read = read
        self._until = until

    def stop(self):
        try:
            self.records.append(self._read())
        except Exception as error:              # noqa: BLE001 - stop and report
            self.error = "%s: %s" % (type(error).__name__, error)
            return True
        if len(self.records) > 200:
            self.capped = True
            return True
        if self._until(self.records):
            self.satisfied = True
            return True
        return False


class _Heartbeat(gdb.Breakpoint):
    """~25 Hz auto-continuing breakpoint (ble_send, fired by dri_transmit
    straight from loop()) that stops the target for real once a wall-clock
    deadline passes.

    ble_send is independent of the TaskScheduler, so the heartbeat keeps
    running even when the scheduler itself is what a regression broke. The
    deadline is only evaluated while the target runs, so time spent halted
    on other breakpoints (scenario logic) does not count. A missed sync
    point therefore fails with a RuntimeError instead of hanging until the
    global run timeout with no diagnostics."""

    def __init__(self, deadline):
        super().__init__("ble_send")
        self.silent = True
        self.deadline = deadline
        self.timed_out = False

    def stop(self):
        if time.time() >= self.deadline:
            self.timed_out = True
            return True
        return False


SCENARIOS = []                                # (title, function), registration order


def scenario(title):
    """Register a scenario: a function taking the Session."""
    def register(fn):
        SCENARIOS.append((title, fn))
        return fn
    return register
