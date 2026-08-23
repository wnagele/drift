from expectations import ODID_VERSION, SCHEDULE_TO_MESSAGE_TYPE, odid_fixture
from harness import scenario

# First byte of the BASIC_ID message: (message type << 4) | protocol version.
BASIC_ID_HEADER = 0x02


# A full schedule is 25 slots; capturing 26 proves the schedule wraps back to
# the BASIC_ID slot afterwards.
CYCLE_LENGTH = 25


def full_cycle(records):
    """True once the records contain a complete 25-slot schedule starting at
    the BASIC_ID slot, plus the slot that follows it."""
    return any(records[i][1][0] == BASIC_ID_HEADER and len(records) - i >= CYCLE_LENGTH + 1
               for i in range(len(records)))


def cycle_of(sends):
    """The first complete 25-slot schedule in the captured sends, plus the
    slot that follows it."""
    for i in range(len(sends)):
        if sends[i][1][0] == BASIC_ID_HEADER and len(sends) - i >= CYCLE_LENGTH + 1:
            return sends[i:i + CYCLE_LENGTH + 1]
    return None


def two_packs(records):
    """True once two message packs have been captured."""
    return len(records) >= 2


@scenario("broadcast: the full 25-slot ODID schedule goes out over BLE")
def broadcast(t):
    # Arm the aircraft with good telemetry, matching the reference aircraft
    # the ODID fixtures encode (full location incl. height and direction).
    t.replay("armed_fix")
    t.run_to("dri_update_location")       # GLOBAL_POSITION_INT, the last message in the fixture

    sends = t.ble_sends(full_cycle)
    cycle = cycle_of(sends)
    if cycle is None:
        t.check_eq("a full 25-slot schedule captured", len(sends), CYCLE_LENGTH)
        return

    # Schedules 1-4 carry the identity and system messages of the configured
    # reference aircraft, 5-25 location, then the schedule wraps back to 1.
    headers = [(t.eval(name) << 4) | ODID_VERSION
               for _, name in sorted(SCHEDULE_TO_MESSAGE_TYPE.items())]
    t.check_eq("schedule headers", [record[1][0] for record in cycle[:CYCLE_LENGTH]],
               headers + [headers[4]] * 20)
    t.check_eq("schedule wraps back to the BASIC_ID slot",
               cycle[CYCLE_LENGTH][1][0], BASIC_ID_HEADER)

    counters = [record[0] for record in cycle]
    t.check_eq("message counters increment", counters[1:],
               [(counter + 1) % 256 for counter in counters[:-1]])

    # Broadcast rate: one slot per DRI_INTERVAL * DRI_GUARD_MULTIPLIER (40 ms)
    # guard interval, a full 25-slot cycle per second. For a Remote ID device
    # the rate is a compliance property; deleting the dri_due() guard (fire
    # as fast as loop() spins) used to survive every tier.
    deltas = [later[2] - earlier[2] for earlier, later in zip(cycle, cycle[1:])]
    t.check_eq("slot interval is the 40 ms guard interval",
               all(35 <= delta <= 60 for delta in deltas), True)
    cycle_time = cycle[CYCLE_LENGTH][2] - cycle[0][2]
    t.check_eq("full 25-slot cycle takes ~1 s", 950 <= cycle_time <= 1200, True)

    # The identity slots carry the configured aircraft, byte-exact against
    # the reference encodings.
    t.check_eq("basic id slot matches the reference encoding",
               cycle[0][1], odid_fixture("basic_id"))
    t.check_eq("self id slot matches the reference encoding",
               cycle[1][1], odid_fixture("self_id"))
    t.check_eq("operator id slot matches the reference encoding",
               cycle[2][1], odid_fixture("operator_id"))

    t.check_eq("system slot matches the reference encoding",
               cycle[3][1], odid_fixture("system"))
    t.check_eq("every location slot matches the reference encoding",
               [record[1] for record in cycle[4:CYCLE_LENGTH]],
               [odid_fixture("location")] * (CYCLE_LENGTH - 4))


@scenario("broadcast: the BLE5 message pack carries every valid message")
def broadcast_pack(t):
    # The pack path runs alongside the slot schedule (~1 Hz instead of one
    # slot per 40 ms). With the reference aircraft fully configured and the
    # telemetry and take-off origin from the earlier scenarios, the pack is
    # the five reference messages behind the 3-byte pack header - byte-exact
    # against the same fixtures the BT4 slots are compared to, in the
    # vendored builder's order (Basic ID, Location, Self-ID, System,
    # Operator ID).
    packs = t.ble_pack_sends(two_packs)

    expected = (bytes([0xF2, 25, 5])
                + odid_fixture("basic_id") + odid_fixture("location")
                + odid_fixture("self_id") + odid_fixture("system")
                + odid_fixture("operator_id"))
    for i, (counter, pack, length, _) in enumerate(packs[:2]):
        t.check_eq("pack %d content" % i, (pack, length), (expected, len(expected)))
    t.check_eq("pack counters increment",
               packs[1][0], (packs[0][0] + 1) % 256)

    # Cadence: one pack per DRI_PACK_INTERVAL (1 s).
    delta = packs[1][3] - packs[0][3]
    t.check_eq("pack interval is ~1 s", 950 <= delta <= 1200, True)
