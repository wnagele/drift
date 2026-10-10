#!/usr/bin/env python3
"""Burn the DRIFT board identity (board type + revision + the optional
manufacturer serial) into the ESP32-C3 eFuse BLOCK3 user data, as the
manufacturing step on the jig.

The layout (bytes: type, revision major, revision minor, then a
NUL-padded ASCII serial of up to 20 characters) matches src/board.{h,cpp},
and only the identity is stored there - pins are resolved in firmware, never
in eFuse. Byte 0 = 0 is reserved for UNDEFINED, and an all-zero serial
region reads back as "no serial", so a block that was never burned reports
no identity and no serial rather than guessing.

A burned serial is the module's registered identity in the firmware: it
becomes the config ua_id default, the dash shows it read-only, and
config_save() refuses any attempt to change it.

espefuse (esptool >= v5) is driven through the custom eFuse table
tools/drift_efuse_table.csv, so the burn uses named fields and this script
never hand-computes block byte order: espefuse places the fields LSB-first
within the block, which is the order esp_efuse_read_block() reads and
board.cpp decodes. The summary read-back after burning verifies the burn
through the tool's own reader.

The serial must be printable ASCII without the comma - the ASTM F3411
serial-number character set - and at most 20 characters (ODID_ID_SIZE).

BLOCK3 on the ESP32-C3 is RS(4:2)-coded, which means it can be burned
exactly ONCE - the identity cannot be corrected or extended afterwards,
not even by adding bits (espefuse refuses). The script therefore refuses
to touch a block that holds anything already, and treats "already holds
the wanted identity" as success so re-running the jig is safe. Burn the
serial together with the type and revision: there is no second burn.

By default the block is additionally left NOT write-protected, so nothing
is policy-locked in the average/dev case. Production batches pass
--write-protect, which burns the BLOCK3 write-disable bit after the
identity burn and locks the identity for good.

Examples:

  burn one board (espefuse asks for confirmation, no write-protect):
    python3 tools/burn_board_id.py --port /dev/cu.usbmodem2101 \\
        --board ADVANCED --revision 1.2 --serial 1DRIFT24A00000001

  production batch (write-protect, no prompts):
    python3 tools/burn_board_id.py --port /dev/cu.usbmodem2101 \\
        --board ADVANCED --revision 1.2 --serial 1DRIFT24A00000001 \\
        --write-protect --yes

  offline dry run against a virtual eFuse file (no chip needed):
    python3 tools/burn_board_id.py --efuse-file ef.bin \\
        --board BASIC --revision 0.1 --yes
"""

import argparse
import os
import re
import shutil
import subprocess
import sys

CHIP = "esp32c3"
BLOCK = "BLOCK_USR_DATA"
CSV = os.path.join(os.path.dirname(os.path.abspath(__file__)), "drift_efuse_table.csv")

# Mirror of board_type_t in src/board.h. 0 is UNDEFINED (unburned/unknown)
# and is not a burnable choice.
BOARD_TYPES = {"BASIC": 1, "ADVANCED": 2}
TYPE_NAMES = {v: k for k, v in BOARD_TYPES.items()}

# Serial length cap (ODID_ID_SIZE in the firmware) and the ASTM F3411
# serial-number character set: printable ASCII without the comma.
SERIAL_MAX = 20
SERIAL_FORBIDDEN = ","


def serial_fields():
    return tuple("DRIFT_SERIAL_%02d" % i for i in range(SERIAL_MAX))


FIELDS = ("DRIFT_BOARD_TYPE", "DRIFT_REV_MAJOR", "DRIFT_REV_MINOR")
BLOCK_RE = re.compile(
    r"^%s\s+\(BLOCK3\b.*?dump:\s+((?:[0-9a-fA-F]{8}\s+)+[0-9a-fA-F]{8})" % BLOCK
)
FIELD_RE = r"^%s\s+\(BLOCK3\).*?=\s*(\S+)"


def parse_serial(s):
    serial = s.strip() if s else s
    if serial is None or serial == "":
        return None
    if len(serial) > SERIAL_MAX:
        raise ValueError("serial must be at most %d characters" % SERIAL_MAX)
    for ch in serial:
        if not ch.isprintable() or ch in SERIAL_FORBIDDEN:
            raise ValueError(
                "serial may only contain printable ASCII without %r (got %r)"
                % (SERIAL_FORBIDDEN, ch)
            )
    return serial


def parse_revision(s):
    m = re.fullmatch(r"(\d{1,3})\.(\d{1,3})", s.strip())
    if not m:
        raise ValueError("revision must be MAJOR.MINOR, e.g. 1.2")
    major, minor = int(m.group(1)), int(m.group(2))
    if major > 255 or minor > 255:
        raise ValueError("revision parts must each fit one byte (0-255)")
    return major, minor


def find_espefuse(override):
    if override:
        return override.split()
    if shutil.which("espefuse"):
        return ["espefuse"]
    return [sys.executable, "-m", "espefuse"]


def cmd_prefix(args):
    cmd = find_espefuse(args.espefuse) + ["--chip", CHIP]
    if args.port:
        cmd += ["-p", args.port]
    else:
        cmd += ["--virt", "--path-efuse-file", args.efuse_file]
    cmd += ["--extend-efuse-table", CSV]
    if args.yes:
        cmd += ["--do-not-confirm"]
    return cmd


def run(args, subcommand):
    cmd = cmd_prefix(args) + subcommand
    result = subprocess.run(cmd, capture_output=True, text=True)
    output = (result.stdout or "") + (result.stderr or "")
    if result.returncode != 0:
        sys.stderr.write(output)
        sys.exit("error: espefuse %s failed" % " ".join(subcommand))
    return output


def read_block(args):
    """The raw BLOCK3 words, as a hex string ('' for an unburned block)."""
    for line in run(args, ["dump"]).splitlines():
        m = BLOCK_RE.match(line)
        if m:
            words = m.group(1).split()
            if any(int(w, 16) for w in words):
                return m.group(1)
            return ""
    sys.exit("error: espefuse dump did not report %s" % BLOCK)


def read_value(args, field):
    """One named field's current value from the summary, as an int."""
    for line in run(args, ["summary", field]).splitlines():
        m = re.match(FIELD_RE % field, line)
        if m:
            tok = m.group(1)
            return int(tok, 16 if tok.startswith("0x") else 10)
    sys.exit("error: espefuse summary did not report %s" % field)


def read_identity(args):
    """The three identity fields, as (type, major, minor)."""
    return tuple(read_value(args, f) for f in FIELDS)


def read_serial(args):
    """The burned serial, as a string ('' when the region is unburned)."""
    chars = []
    for field in serial_fields():
        value = read_value(args, field)
        if value == 0:
            break  # NUL-padded: 0 is the end of the serial proper
        chars.append(chr(value))
    return "".join(chars)


def describe(type_, major, minor):
    return "board %s rev %d.%d" % (TYPE_NAMES.get(type_, "UNDEFINED"), major, minor)


def main():
    parser = argparse.ArgumentParser(
        epilog=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--board", required=True, choices=sorted(BOARD_TYPES))
    parser.add_argument("--revision", required=True, type=parse_revision,
                        metavar="MAJOR.MINOR", help="e.g. 1.2")
    parser.add_argument("--serial", type=parse_serial, metavar="SERIAL",
                        help="optional manufacturer serial (printable ASCII "
                             "without ',', at most 20 chars). Burns together "
                             "with the identity - there is no second burn - "
                             "and locks ua_id in the firmware.")
    port_group = parser.add_mutually_exclusive_group(required=True)
    port_group.add_argument("--port", help="serial port of the board to burn")
    port_group.add_argument("--efuse-file",
                            help="virtual eFuse file (offline, no chip)")
    parser.add_argument("--write-protect", action="store_true",
                        help="burn the BLOCK3 write-disable bit after the "
                             "identity, locking it for good. Off by default.")
    parser.add_argument("--yes", action="store_true",
                        help="skip espefuse's burn confirmation prompt")
    parser.add_argument("--espefuse", help="espefuse command override "
                        "(default: espefuse on PATH, else python3 -m espefuse)")
    args = parser.parse_args()

    def say(text):
        print(text, flush=True)

    major, minor = args.revision
    type_ = BOARD_TYPES[args.board]
    serial = args.serial
    wanted = (type_, major, minor)
    say("burning %s%s into %s" % (
        describe(*wanted),
        " serial %s" % serial if serial else "",
        BLOCK,
    ))

    block = read_block(args)
    current = read_identity(args)
    current_serial = read_serial(args)
    if block:
        # RS(4:2) coding: one burn per block, ever. Refuse anything that is
        # not the wanted identity already sitting there.
        if current == wanted and current_serial == (serial or ""):
            say("already burned: %s%s - nothing to do" % (
                describe(*current),
                " serial %s" % current_serial if current_serial else "",
            ))
        else:
            say("BLOCK3 is burned: %s%s" % (
                describe(*current),
                " serial %s" % current_serial if current_serial else "",
            ))
            say("raw block: %s" % block)
            sys.exit("refusing: the C3's BLOCK3 is RS-coded and can be burned "
                     "only once - a burned identity cannot be corrected or "
                     "extended. Replace the module.")
    else:
        # espefuse refuses to "burn" a field value of 0 (nothing would burn).
        # The pre-check guarantees a fresh block, so a 0 field already reads
        # as the wanted value: pass only the nonzero fields. The serial bytes
        # are all nonzero by the charset check, and its NUL padding stays
        # unburned, which is exactly how the firmware decodes it.
        pairs = [tok for field, value in zip(FIELDS, wanted) if value
                 for tok in (field, str(value))]
        if serial:
            pairs += [tok for i, ch in enumerate(serial)
                      for tok in ("DRIFT_SERIAL_%02d" % i, str(ord(ch)))]
        run(args, ["burn-efuse"] + pairs)

        readback = read_identity(args)
        if readback != wanted:
            sys.exit("error: read-back mismatch: burned %s, read back %s"
                     % (describe(*wanted), describe(*readback)))
        readback_serial = read_serial(args)
        if readback_serial != (serial or ""):
            sys.exit("error: read-back mismatch: serial %r, read back %r"
                     % (serial, readback_serial))
        say("burned: %s%s" % (
            describe(*readback),
            " serial %s" % readback_serial if readback_serial else "",
        ))

    if args.write_protect:
        run(args, ["write-protect-efuse", BLOCK])
        say("write-protected: %s can no longer be changed" % BLOCK)


if __name__ == "__main__":
    main()