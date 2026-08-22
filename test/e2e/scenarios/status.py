from harness import scenario


@scenario("status: the scheduler fires the 3 s process and 1 s send tasks")
def scheduled_status(t):
    # Free-run the target and observe net_broadcast (stubbed in the e2e
    # build): the 1 s taskSendStatus pushes the status over WS, the 3 s
    # taskProcessStatus folds the counters into the flags. This scenario used
    # to call status_process() directly, which left a wiring regression - a
    # task never registered with the scheduler - completely untested (the
    # status-task-not-registered mutant survived the whole suite).
    def read():
        return (
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
        return last[2] == 0 and last[3] == 0 and last[0] != 0 and last[1] != 0

    records = t.collect_calls("net_broadcast", read, processed)

    # The 1 s task fired at least three times before the 3 s task processed.
    t.check_eq("net_broadcast fired at least three times", len(records) >= 3, True)
    telemetry, gnss, telemetry_count, gnss_count, _ = records[-1]
    t.check_eq("flags folded from the counters by the 3 s task",
               (telemetry, gnss), (1, 1))
    t.check_eq("counters reset by the 3 s task",
               (telemetry_count, gnss_count), (0, 0))

    # The send task fires once per second of guest time (the heartbeat the
    # harness itself rides on).
    deltas = [later[4] - earlier[4] for earlier, later in zip(records, records[1:])]
    t.check_eq("broadcast interval is 1 s",
               all(990 <= delta <= 1010 for delta in deltas), True)
