#ifndef DRIFT_BOARD_H
#define DRIFT_BOARD_H

#include <stdint.h>

// Board identity lives in the eFuse BLOCK3 user data (BLOCK_USR_DATA), burned
// once on the manufacturing jig (tools/burn_board_id.py, which verifies the
// burn with a read-back) and never written again - literally: BLOCK3 is
// RS(4:2)-coded on the C3, which allows exactly one burn, so the identity
// cannot be corrected or extended afterwards, not even by adding bits.
// Only the identity is stored there - never the pin map: what a board type
// means in pins is resolved in firmware (see led.cpp's table), where it can
// follow hardware revisions without touching eFuse at all. The jig burn
// leaves the BLOCK3 write-disable bit alone by default; a production batch
// passes --write-protect, which is the one step that locks the block for
// good.
//
// eFuse burns bits 0->1 only, so a virgin block reads all-zero; byte 0 is the
// board type and 0 is reserved for UNDEFINED, which keeps a board lacking
// eFuse details (never burned, or a blank in a QEMU / native stub)
// distinguishable from a real type without a magic value or checksum. Any
// unknown type byte also resolves to UNDEFINED rather than guessing.
//
// Jig burn (all identity bytes in the one invocation):
//   python3 tools/burn_board_id.py --port PORT --board BASIC --revision 2.1
// Production batches add --write-protect.
//
// Test-build overrides: eFuse burns are irreversible, so bench testing a
// board identity without sacrificing a devkit to it is done at compile time,
// not by burning. Define the identity and board_init() fabricates the raw
// bytes instead of reading eFuse - through the same decode path, so the
// running firmware is byte-for-byte what a burn of the same identity would
// produce:
//   PLATFORMIO_BUILD_FLAGS="-DDRIFT_BOARD_OVERRIDE_TYPE=3 \
//       -DDRIFT_BOARD_OVERRIDE_REV_MAJOR=1 -DDRIFT_BOARD_OVERRIDE_REV_MINOR=2 \
//       -DDRIFT_BOARD_OVERRIDE_SERIAL=1DRIFT24A00000001" \
//     pio run -e esp32_c3_devkit
// BASIC=1, ADVANCED=2 (0 is UNDEFINED, the un-overridden state).
// The revision macros default to 0; DRIFT_BOARD_OVERRIDE_SERIAL (a bare
// token, stringized in board.cpp - no shell quoting) additionally
// fabricates a manufacturer serial, so the ua_id lock can be exercised
// too. It requires DRIFT_BOARD_OVERRIDE_TYPE: the whole raw block is
// fabricated, never mixed with a read one, so SERIAL alone is a compile
// error rather than a silent fall-back to eFuse.
// Never in a production build: an override masks whatever identity
// the eFuse really holds.
#define BOARD_TYPE_BYTES 1
#define BOARD_REV_MAJOR_BYTES 1
#define BOARD_REV_MINOR_BYTES 1
#define BOARD_EFUSE_BYTES (BOARD_TYPE_BYTES + BOARD_REV_MAJOR_BYTES + BOARD_REV_MINOR_BYTES)
#define BOARD_EFUSE_BITS (BOARD_EFUSE_BYTES * 8)

// The optional manufacturer serial, following the identity in BLOCK3:
// NUL-padded plain ASCII, up to ODID_ID_SIZE (20) characters. An all-zero
// region reads back as "no serial" - the same absent-means-undefined
// convention as the type byte - so a blank under QEMU and the native stub
// stay on the unlocked path. A burned serial *is* the module's registered
// identity (config's ua_id): it is forced into ua_id at every boot, the
// dash shows it read-only, and config_save() rejects any attempt to change it.
#define BOARD_SERIAL_OFFSET BOARD_EFUSE_BYTES   // bytes: type, rev major, rev minor
#define BOARD_SERIAL_BYTES 20                  // ODID_ID_SIZE
#define BOARD_SERIAL_EFUSE_BYTES (BOARD_SERIAL_OFFSET + BOARD_SERIAL_BYTES)
#define BOARD_SERIAL_EFUSE_BITS (BOARD_SERIAL_EFUSE_BYTES * 8)

typedef enum {
    BOARD_TYPE_UNDEFINED = 0,  // unburned block, unknown type, or no eFuse
    BOARD_TYPE_BASIC = 1,     // bare minimum, like the devkit build
    BOARD_TYPE_ADVANCED = 2,  // buck converter + two status LEDs
} board_type_t;

typedef struct {
    board_type_t type;
    uint8_t rev_major;
    uint8_t rev_minor;
} BoardId;

// Decode the raw BLOCK3 bytes. Pure: the eFuse read is board_init()'s job.
BoardId board_decode(const uint8_t raw[BOARD_EFUSE_BYTES]);
const char *board_type_name(board_type_t type);

// Decode the serial region (bytes 3..22, NUL-padded) into a NUL-terminated
// string. Returns false when no serial is burned (the all-zero region); out
// is "" then.
bool board_decode_serial(const uint8_t raw[BOARD_SERIAL_EFUSE_BYTES],
                         char out[BOARD_SERIAL_BYTES + 1]);

// Read the identity once at boot and cache it.
void board_init(void);
BoardId board_id(void);
board_type_t board_type(void);

// The burned serial ("" when none), and whether one is burned at all.
const char *board_serial(void);
bool board_serial_present(void);

#if !defined(ESP32)
// Native tests: the eFuse stub burns nothing, so tests inject a serial to
// exercise the config lock. Absent on the device.
void board_set_serial_for_test(const char *serial);
#endif

#endif