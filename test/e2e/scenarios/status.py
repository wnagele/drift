import json

from harness import scenario


@scenario("status: the scheduler fires the 3 s process and 1 s send tasks")
def scheduled_status(t):
    # Free-run the target and observe net_broadcast (stubbed in the e2e
    # build): the 1 s taskSendStatus pushes both /ws messages, the 3 s
    # taskProcessStatus folds the counters into the flags. This scenario used
    # to call status_process() directly, which left a wiring regression - a
    # task never registered with the scheduler - completely untested (the
    # status-task-not-registered mutant survived the whole suite).
    #
    # The pushed payload itself is read out of the argument register, so the
    # two message types are told apart by what actually went to the socket
    # rather than by counting calls.
    def read():
        return (
            t.read_arduino_string_ref("$a0"),
            t.eval("telemetry"),
            t.eval("gnss"),
            t.eval("telemetry_count"),
            t.eval("gnss_count"),
            t.eval("millis()"),
        )

    def processed(records):
        last = records[-1]
        # The 3 s task has run: the pending counters were folded into the
        # flags (both true from the telemetry scenario's heartbeat and
        # position) and reset.
        return last[3] == 0 and last[4] == 0 and last[1] != 0 and last[2] != 0

    records = t.collect_calls("net_broadcast", read, processed)

    # The 1 s task fired at least three times before the 3 s task processed.
    t.check_eq("net_broadcast fired at least three times", len(records) >= 3, True)
    _, telemetry, gnss, telemetry_count, gnss_count, _ = records[-1]
    t.check_eq("flags folded from the counters by the 3 s task",
               (telemetry, gnss), (1, 1))
    t.check_eq("counters reset by the 3 s task",
               (telemetry_count, gnss_count), (0, 0))

    # Every tick pushes two messages on the one socket: device health and
    # transmit rates, then the broadcast inspector payload. One task, two
    # message types - the dash tells them apart by "type". Counted from the
    # first status message, because the capture can start mid-tick.
    types = [json.loads(record[0])["type"] for record in records]
    start = types.index("status") if "status" in types else 0
    t.check_eq("each tick pushes a status message then a broadcast message",
               types[start:start + 4], ["status", "broadcast"] * 2)

    # The send task fires once per second of guest time (the heartbeat the
    # harness itself rides on). Measured between consecutive status
    # messages, since a tick now carries two.
    stamps = [record[5] for record in records
              if json.loads(record[0])["type"] == "status"]
    deltas = [later - earlier for earlier, later in zip(stamps, stamps[1:])]
    t.check_eq("broadcast interval is 1 s",
               all(990 <= delta <= 1010 for delta in deltas), True)
    # And both messages of a tick go out back to back, not half a second
    # apart: they are two calls in one task run.
    pairs = [(record[0], record[5]) for record in records]
    gaps = [later[1] - earlier[1] for earlier, later in zip(pairs, pairs[1:])
            if json.loads(earlier[0])["type"] == "status"]
    t.check_eq("both messages of a tick are pushed back to back",
               all(gap <= 20 for gap in gaps), True)
