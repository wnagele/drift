#!/bin/sh
# Merge the PlatformIO build output into flash images with esptool, boot them
# in Espressif's QEMU (ESP32-C3) and run test/e2e/firmware.test.py against
# them under gdb. Exits non-zero on a failed check, dumping the emulated
# UART0 log of every failed run.
#
# Three boots, two images:
#   1. the configured aircraft  - NVS pre-seeded with the reference config
#      (nvs_seed.py), the shared-state narrative scenarios;
#   2. factory defaults         - the same firmware on an erased NVS
#      partition: config_init() must write the defaults (power cycle 1);
#   3. factory defaults again   - the same image file, i.e. whatever boot 2
#      committed to flash: the defaults must read back (power cycle 2, the
#      persistence property that was silently broken under QEMU before the
#      DIO flash-mode fix).
#
# The two UARTs map to separate QEMU chardevs: UART0 carries the ROM boot log
# and firmware STDOUT into a per-run serial log, UART1 carries the MAVLink
# telemetry input on a TCP socket the test script connects to.
#
# Flash size note: the esp32c3 machine picks the emulated SPI flash chip purely
# from the image size (hw/riscv/esp32c3.c): 2MB -> w25x16, 4MB -> gd25q32,
# 8MB -> gd25q64, 16MB -> is25lp128. QEMU's m25p80 model does not emulate the
# Quad-Enable status bit for GigaDevice parts, and the Arduino ESP-IDF libs are
# built with CONFIG_ESPTOOLPY_FLASHMODE_QIO, so a 4MB/8MB image aborts in
# esp_flash_init ("assert failed: do_core_init startup.c:328"). Padding to 16MB
# selects the ISSI model, which does emulate QE. The image headers keep the
# real 4MB flash size, so the partition layout is unchanged.
set -eu

WORKSPACE="${DRIFT_WORKSPACE:-/work}"
BUILD_DIR="${BUILD_DIR:-$WORKSPACE/.pio/build/esp32_c3_e2e}"
FLASH_SIZE="${FLASH_SIZE:-16MB}"
IMG="${IMG:-/tmp/flash_image.bin}"
DEFAULTS_IMG="${DEFAULTS_IMG:-/tmp/flash_image_defaults.bin}"
TIMEOUT="${TIMEOUT:-120}"
GDB_PORT="${DRIFT_GDB_PORT:-1234}"
MAVLINK_PORT="${DRIFT_MAVLINK_PORT:-5555}"
QEMU_LOG=/tmp/qemu.log

TEST="$WORKSPACE/test/e2e/firmware.test.py"
[ -f "$TEST" ] || { echo "missing $TEST" >&2; exit 1; }

for f in bootloader.bin partitions.bin firmware.bin; do
    [ -f "$BUILD_DIR/$f" ] || { echo "missing $BUILD_DIR/$f (run: pio run -e esp32_c3_e2e)" >&2; exit 1; }
done

# Pre-seed the NVS partition (0x9000, see min_spiffs.csv) with the reference
# aircraft's config from the shared API fixture, so the firmware boots
# configured (narrative image) - and build its factory-fresh counterpart, an
# erased (all 0xFF) partition of the same size, for the defaults boots.
NVS_SEED=/tmp/nvs-seed.bin
BLANK_NVS=/tmp/nvs-blank.bin
python3 "$WORKSPACE/test/e2e/nvs_seed.py" "$NVS_SEED" 0x5000
dd if=/dev/zero bs=1 count=$((0x5000)) 2>/dev/null | tr '\000' '\377' > "$BLANK_NVS"

merge_image() { # $1 = output image, $2 = NVS partition image
    out_image=$1
    nvs_image=$2
    set -- 0x0 "$BUILD_DIR/bootloader.bin" 0x8000 "$BUILD_DIR/partitions.bin" 0x9000 "$nvs_image"
    # boot_app0 (otadata) lives in the Arduino framework package, not in the
    # build dir. It is optional: with blank otadata the bootloader falls back
    # to app0.
    if [ -n "${BOOT_APP0:-}" ] && [ -f "${BOOT_APP0}" ]; then
        set -- "$@" 0xe000 "$BOOT_APP0"
    fi
    set -- "$@" 0x10000 "$BUILD_DIR/firmware.bin"
    esptool.py --chip esp32c3 merge_bin --fill-flash-size "$FLASH_SIZE" -o "$out_image" "$@"
}

merge_image "$IMG" "$NVS_SEED"
merge_image "$DEFAULTS_IMG" "$BLANK_NVS"

QEMU_PID=""
cleanup() { [ -n "$QEMU_PID" ] && kill -9 "$QEMU_PID" 2>/dev/null || true; }
trap cleanup EXIT INT TERM

# run_suite <image> <serial log> <DRIFT_SCENARIOS> <label> <gdb port> <mavlink port>
run_suite() {
    # -S holds the CPU until gdb continues, so no output can be missed. The
    # watchdogs are disabled because guest time stands still on a breakpoint.
    # nowait on the MAVLink socket is safe: the CPU is frozen at the reset
    # vector until the test script has connected.
    qemu-system-riscv32 \
        -nographic \
        -machine esp32c3 \
        -icount 3 \
        -no-reboot \
        -monitor none \
        -drive file="$1",if=mtd,format=raw \
        -global driver=esp32c3.gpio,property=strap_mode,value=0x08 \
        -global driver=timer.esp32c3.timg,property=wdt_disable,value=true \
        -gdb tcp::$5 -S \
        -serial "file:$2" \
        -serial "tcp::$6,server=on,nowait" \
        >"$QEMU_LOG" 2>&1 &
    QEMU_PID=$!

    RC=0
    DRIFT_WORKSPACE="$WORKSPACE" DRIFT_MAVLINK_PORT="$6" \
        DRIFT_SERIAL_LOG="$2" DRIFT_SCENARIOS="$3" DRIFT_RUN_LABEL="$4" \
        DRIFT_GDB_TARGET=":$5" \
        timeout -s KILL "$TIMEOUT" \
        gdb-multiarch -q -batch -x "$TEST" "$BUILD_DIR/firmware.elf" || RC=$?

    kill -9 "$QEMU_PID" 2>/dev/null || true
    wait "$QEMU_PID" 2>/dev/null || true
    QEMU_PID=""

    if [ "$RC" != "0" ]; then
        echo
        echo "--- emulated UART0 output ($4) ---"
        cat "$2"
        if [ -s "$QEMU_LOG" ]; then
            echo "--- qemu ---"
            cat "$QEMU_LOG"
        fi
    fi
    return "$RC"
}

# The narrative suite shares one boot and accumulates state; the defaults
# suite (defaults + serial audit) boots its own image, twice.
MAIN_SCENARIOS="scenarios.boot,scenarios.telemetry,scenarios.branches,scenarios.status,scenarios.operator,scenarios.broadcast,scenarios.unknown,scenarios.serial"
DEFAULTS_SCENARIOS="scenarios.defaults,scenarios.serial"

FAILED=0
run_suite "$IMG" /tmp/serial.log "$MAIN_SCENARIOS" \
    "configured aircraft" "$GDB_PORT" "$MAVLINK_PORT" || FAILED=1
run_suite "$DEFAULTS_IMG" /tmp/serial-defaults-1.log "$DEFAULTS_SCENARIOS" \
    "power cycle 1: factory NVS" "$((GDB_PORT + 1))" "$((MAVLINK_PORT + 1))" || FAILED=1
run_suite "$DEFAULTS_IMG" /tmp/serial-defaults-2.log "$DEFAULTS_SCENARIOS" \
    "power cycle 2: NVS as committed by cycle 1" "$((GDB_PORT + 2))" "$((MAVLINK_PORT + 2))" || FAILED=1

exit "$FAILED"
