import struct

from expectations import ODID_VERSION, SCHEDULE_TO_MESSAGE_TYPE, odid_fixture
from harness import scenario

# First byte of the BASIC_ID message: (message type << 4) | protocol version.
BASIC_ID_HEADER = 0x02

# The 10-slot cycle's first header bytes ((message type << 4) | protocol
# version), in schedule order. Basic ID appears twice per cycle (slots 1 and
# 7), so a Basic ID header alone cannot anchor the cycle start - the full
# sequence pins it to slot 1 (mirrors dri_slot_type()).
CYCLE_HEADERS = [0x02, 0x12, 0x42, 0x12, 0x32, 0x12,
                 0x02, 0x12, 0x42, 0x52]

# A full schedule is 10 slots; capturing the 11th proves the schedule wraps
# back to the BASIC_ID slot afterwards.
CYCLE_LENGTH = 10


def full_cycle(records):
    """True once the records contain a complete 10-slot schedule starting at
    its first slot, plus the slot that follows it."""
    seq = [record[1][0] for record in records]
    return any(seq[i:i + CYCLE_LENGTH + 1] == CYCLE_HEADERS + [CYCLE_HEADERS[0]]
               for i in range(len(seq) - CYCLE_LENGTH))


def cycle_of(sends):
    """The first complete 10-slot schedule in the captured sends, plus the
    slot that follows it."""
    seq = [record[1][0] for record in sends]
    for i in range(len(seq) - CYCLE_LENGTH):
        if seq[i:i + CYCLE_LENGTH + 1] == CYCLE_HEADERS + [CYCLE_HEADERS[0]]:
            return sends[i:i + CYCLE_LENGTH + 1]
    return None


def two_packs(records):
    """True once two message packs have been captured."""
    return len(records) >= 2


def two_wifi_beacons(records):
    """True once two Wi-Fi beacon refreshes have been captured."""
    return len(records) >= 2


def two_wifi_nan_frames(records):
    """True once two Wi-Fi NAN frames have been captured."""
    return len(records) >= 2


# --- Wi-Fi NAN frame layout (lib/libopendroneid/wifi.c) ----------------------

# The ASTM Remote ID spec pins the NAN cluster id; the network id is the NAN
# spec's fixed destination for service discovery frames.
NAN_CLUSTER_ID = bytes([0x50, 0x6F, 0x9A, 0x01, 0x00, 0xFF])
NAN_NETWORK_ID = bytes([0x51, 0x6F, 0x9A, 0x01, 0x00, 0x00])
WIFI_ALLIANCE_OUI = bytes([0x50, 0x6F, 0x9A])
# SHA-256 hash of "org.opendroneid.remoteid" (first 6 bytes).
ODID_SERVICE_ID = bytes([0x88, 0x69, 0x19, 0x9D, 0x92, 0x09])


def wifi_nan_action_frame(mac, counter, pack):
    """Expected bytes of odid_wifi_build_message_pack_nan_action_frame()."""
    service_info_length = 1 + len(pack)
    frame = struct.pack("<HH", 0x00D0, 0)                 # mgmt action header
    frame += NAN_NETWORK_ID + mac + NAN_CLUSTER_ID + struct.pack("<H", 0)
    frame += bytes([0x04, 0x09]) + WIFI_ALLIANCE_OUI + bytes([0x13])
    frame += bytes([0x03]) + struct.pack("<H", 10 + service_info_length)
    frame += ODID_SERVICE_ID + bytes([0x01, 0x00, 0x10, service_info_length])
    frame += bytes([counter]) + pack
    frame += (bytes([0x0E]) + struct.pack("<H", 4) + bytes([0x01])
              + struct.pack("<H", 0x0200) + bytes([counter]))
    return frame


def wifi_nan_sync_beacon(mac):
    """Expected bytes of odid_wifi_build_nan_sync_beacon_frame() except the
    8-byte timestamp, which carries the target's CLOCK_MONOTONIC."""
    frame = struct.pack("<HH", 0x0080, 0)                 # mgmt beacon header
    frame += b"\xFF" * 6 + mac + NAN_CLUSTER_ID + struct.pack("<H", 0)
    frame += b"\x00" * 8 + struct.pack("<HH", 0x0200, 0x0420)
    frame += bytes([0xDD, 0x22]) + WIFI_ALLIANCE_OUI + bytes([0x13])
    frame += bytes([0x00]) + struct.pack("<H", 2) + bytes([0xFE, 0xEA])
    frame += bytes([0x01]) + struct.pack("<H", 13) + mac + bytes([0xEA, 0xFE, 0x00]) + b"\x00" * 4
    frame += bytes([0x02]) + struct.pack("<H", 6) + ODID_SERVICE_ID
    return frame


@scenario("broadcast: the full 10-slot ODID schedule goes out over BLE")
def broadcast(t):
    # Arm the aircraft with good telemetry, matching the reference aircraft
    # the ODID fixtures encode (full location incl. height and direction).
    t.replay("armed_fix")
    t.run_to("dri_update_location")       # GLOBAL_POSITION_INT, the last message in the fixture

    sends = t.ble_sends(full_cycle)
    cycle = cycle_of(sends)
    if cycle is None:
        t.check_eq("a full 10-slot schedule captured", len(sends), CYCLE_LENGTH)
        return

    # The 10-slot cycle allocates the F3411 message rates: Basic, Loc,
    # System, Loc, SelfID, Loc, Basic, Loc, System, OpID - then the
    # schedule wraps back to the Basic ID slot.
    headers = [(t.eval(name) << 4) | ODID_VERSION
               for _, name in sorted(SCHEDULE_TO_MESSAGE_TYPE.items())]
    t.check_eq("schedule headers", [record[1][0] for record in cycle[:CYCLE_LENGTH]],
               headers)
    t.check_eq("schedule wraps back to the BASIC_ID slot",
               cycle[CYCLE_LENGTH][1][0], BASIC_ID_HEADER)

    counters = [record[0] for record in cycle]
    t.check_eq("message counters increment", counters[1:],
               [(counter + 1) % 256 for counter in counters[:-1]])

    # Broadcast rate: one slot per DRI_SLOT_INTERVAL (100 ms, the Bluetooth
    # Core Spec ADV_NONCONN_IND advertising floor), a full 10-slot cycle per
    # second. For a Remote ID device the rate is a compliance property;
    # deleting the dri_due() guard (fire as fast as loop() spins) used to
    # survive every tier.
    deltas = [later[2] - earlier[2] for earlier, later in zip(cycle, cycle[1:])]
    t.check_eq("slot interval is the 100 ms slot interval",
               all(95 <= delta <= 130 for delta in deltas), True)
    cycle_time = cycle[CYCLE_LENGTH][2] - cycle[0][2]
    t.check_eq("full 10-slot cycle takes ~1 s", 950 <= cycle_time <= 1200, True)

    # The identity/system slots carry the configured aircraft, byte-exact
    # against the reference encodings.
    t.check_eq("basic id slots match the reference encoding",
               [cycle[i][1] for i in (0, 6)], [odid_fixture("basic_id")] * 2)
    t.check_eq("self id slot matches the reference encoding",
               cycle[4][1], odid_fixture("self_id"))
    t.check_eq("operator id slot matches the reference encoding",
               cycle[9][1], odid_fixture("operator_id"))
    t.check_eq("system slots match the reference encoding",
               [cycle[i][1] for i in (2, 8)], [odid_fixture("system")] * 2)
    t.check_eq("every location slot matches the reference encoding",
               [cycle[i][1] for i in (1, 3, 5, 7)],
               [odid_fixture("location")] * 4)


@scenario("broadcast: the BLE5 message pack carries every valid message")
def broadcast_pack(t):
    # The pack path runs alongside the slot schedule (~1 Hz instead of one
    # slot per 100 ms). With the reference aircraft fully configured and the
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


@scenario("broadcast: the Wi-Fi Beacon IE carries every valid message")
def broadcast_wifi_beacon(t):
    # The Wi-Fi Beacon transport refreshes the same message pack as the BT5
    # path, but on its own ~5 Hz cadence (DRI_WIFI_BEACON_INTERVAL, the 5 Hz rate
    # rule the plan pins for Wi-Fi broadcasts). The pack content is the same
    # five reference messages byte-for-byte; only the schedule and the
    # counter differ - the IE framing itself is pinned natively against the
    # vendored beacon frame builder (test_dri).
    wifi_beacons = t.wifi_beacon_sends(two_wifi_beacons)

    expected = (bytes([0xF2, 25, 5])
                + odid_fixture("basic_id") + odid_fixture("location")
                + odid_fixture("self_id") + odid_fixture("system")
                + odid_fixture("operator_id"))
    for i, (counter, pack, length, _) in enumerate(wifi_beacons[:2]):
        t.check_eq("wifi beacon %d content" % i, (pack, length), (expected, len(expected)))
    t.check_eq("wifi beacon counters increment",
               wifi_beacons[1][0], (wifi_beacons[0][0] + 1) % 256)

    # Cadence: one IE refresh per DRI_WIFI_BEACON_INTERVAL (200 ms).
    delta = wifi_beacons[1][3] - wifi_beacons[0][3]
    t.check_eq("wifi beacon interval is ~200 ms", 150 <= delta <= 350, True)


@scenario("broadcast: the Wi-Fi NAN frames carry every valid message")
def broadcast_wifi_nan(t):
    # The Wi-Fi NAN transport injects raw 802.11 management frames on the
    # ~512 ms discovery-window cadence (DRI_WIFI_NAN_INTERVAL, the NAN spec's
    # cluster timing): a sync beacon plus an action frame wrapping the same
    # message pack the BT5 and Wi-Fi Beacon paths broadcast. The expected
    # frames below re-derive the vendored builder's layout around the
    # reference pack; the action-frame counter advances once per transmitted
    # frame and rides both the service info byte and the trailing service
    # update indicator, so it is read back out of each capture instead of
    # assumed.
    wifi_nan_frames = t.wifi_nan_action_sends(two_wifi_nan_frames)

    # The source MAC embedded in the frames is the target's eFuse base MAC
    # (SA field of the mgmt header, offset 10); both captures must agree.
    mac = wifi_nan_frames[0][0][10:16]
    t.check_eq("wifi nan frames share one source MAC",
               wifi_nan_frames[1][0][10:16], mac)

    expected_pack = (bytes([0xF2, 25, 5])
                     + odid_fixture("basic_id") + odid_fixture("location")
                     + odid_fixture("self_id") + odid_fixture("system")
                     + odid_fixture("operator_id"))
    for i, (frame, length, _) in enumerate(wifi_nan_frames[:2]):
        counter = frame[43]
        expected = wifi_nan_action_frame(mac, counter, expected_pack)
        t.check_eq("wifi nan action frame %d content" % i,
                   (frame, length), (expected, len(expected)))
        # The counter duplicates into the trailing service update indicator.
        t.check_eq("wifi nan action frame %d service update indicator" % i,
                   frame[length - 1], counter)
    t.check_eq("wifi nan action frame counters increment",
               wifi_nan_frames[1][0][43], (wifi_nan_frames[0][0][43] + 1) % 256)

    # The sync beacon keeps the NAN cluster alive; only its timestamp (bytes
    # 24-31, CLOCK_MONOTONIC on the target) is not byte-predictable.
    wifi_nan_beacons = t.wifi_nan_sync_sends(two_wifi_nan_frames)
    expected = wifi_nan_sync_beacon(mac)
    for i, (frame, length, _) in enumerate(wifi_nan_beacons[:2]):
        t.check_eq("wifi nan sync beacon %d content" % i,
                   (frame[:24] + frame[32:], length),
                   (expected[:24] + expected[32:], len(expected)))

    # Cadence: one frame pair per DRI_WIFI_NAN_INTERVAL (512 ms).
    delta = wifi_nan_beacons[1][2] - wifi_nan_beacons[0][2]
    t.check_eq("wifi nan beacon interval is ~512 ms", 450 <= delta <= 700, True)
