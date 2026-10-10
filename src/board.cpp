#include "board.h"

#include <string.h>

#if defined(ESP32)
#include "esp_efuse.h"
#endif

// Test-build identity overrides (see board.h): DRIFT_BOARD_OVERRIDE_TYPE is
// the anchor - defined, the identity is fabricated and eFuse is never read.
// The whole raw block is fabricated (there is no partial override mixing a
// fabricated serial into a read type), so SERIAL without TYPE has no
// meaning and is a compile error rather than a silent fall-back to eFuse.
#if defined(DRIFT_BOARD_OVERRIDE_SERIAL) && !defined(DRIFT_BOARD_OVERRIDE_TYPE)
#error "DRIFT_BOARD_OVERRIDE_SERIAL requires -DDRIFT_BOARD_OVERRIDE_TYPE (the type macro anchors the whole override)"
#endif
#if defined(DRIFT_BOARD_OVERRIDE_TYPE)
#ifndef DRIFT_BOARD_OVERRIDE_REV_MAJOR
#define DRIFT_BOARD_OVERRIDE_REV_MAJOR 0
#endif
#ifndef DRIFT_BOARD_OVERRIDE_REV_MINOR
#define DRIFT_BOARD_OVERRIDE_REV_MINOR 0
#endif
#if (DRIFT_BOARD_OVERRIDE_TYPE < 1) || (DRIFT_BOARD_OVERRIDE_TYPE > 2)
#error "DRIFT_BOARD_OVERRIDE_TYPE must be a board type value: BASIC=1, ADVANCED=2 (UNDEFINED=0 is the un-overridden state)"
#endif
// Stringize so the CLI passes the serial as a bare token (no quoting):
// -DDRIFT_BOARD_OVERRIDE_SERIAL=ABC-123
#define DRIFT_STR_(x) #x
#define DRIFT_STR(x) DRIFT_STR_(x)
#endif

static BoardId cached = { BOARD_TYPE_UNDEFINED, 0, 0 };
static char serial_cache[BOARD_SERIAL_BYTES + 1] = { 0 };

BoardId board_decode(const uint8_t raw[BOARD_EFUSE_BYTES]) {
    BoardId id = { BOARD_TYPE_UNDEFINED, raw[1], raw[2] };
    switch (raw[0]) {
        case BOARD_TYPE_BASIC:
        case BOARD_TYPE_ADVANCED:
            id.type = (board_type_t)raw[0];
            break;
        default:
            break;  // 0 = unburned block (see board.h); anything else unknown
    }
    return id;
}

bool board_decode_serial(const uint8_t raw[BOARD_SERIAL_EFUSE_BYTES],
                         char out[BOARD_SERIAL_BYTES + 1]) {
    bool present = false;
    size_t len = 0;
    for (size_t i = BOARD_SERIAL_OFFSET; i < BOARD_SERIAL_EFUSE_BYTES; i++) {
        if (raw[i] == 0)
            break;  // NUL-padded: the rest of the region is unburned padding
        present = true;
        if (len < BOARD_SERIAL_BYTES)
            out[len++] = (char)raw[i];
    }
    out[len] = '\0';
    return present;
}

const char *board_type_name(board_type_t type) {
    switch (type) {
        case BOARD_TYPE_BASIC: return "BASIC";
        case BOARD_TYPE_ADVANCED: return "ADVANCED";
        default: return "UNDEFINED";
    }
}

// Fill the raw BLOCK3 bytes. Whole-body selection: a test build fabricates
// them from the -DDRIFT_BOARD_OVERRIDE_* defines (eFuse never read), the
// device reads the block, native zeroes (no eFuse). One decode path
// downstream either way, so an override build sees exactly what a burn of
// the same identity would produce.
static void board_load_raw(uint8_t raw[BOARD_SERIAL_EFUSE_BYTES]) {
#if defined(DRIFT_BOARD_OVERRIDE_TYPE)
    raw[0] = DRIFT_BOARD_OVERRIDE_TYPE;
    raw[1] = DRIFT_BOARD_OVERRIDE_REV_MAJOR;
    raw[2] = DRIFT_BOARD_OVERRIDE_REV_MINOR;
#if defined(DRIFT_BOARD_OVERRIDE_SERIAL)
    strncpy((char *)raw + BOARD_SERIAL_OFFSET,
            DRIFT_STR(DRIFT_BOARD_OVERRIDE_SERIAL), BOARD_SERIAL_BYTES);
#endif
#elif defined(ESP32)
    // On a read error raw stays zeroed: UNDEFINED and no serial rather than
    // half-read values.
    (void)esp_efuse_read_block(EFUSE_BLK_USER_DATA, raw, 0, BOARD_SERIAL_EFUSE_BITS);
#else // native tests: no eFuse, so no identity - raw stays zeroed
    (void)raw;
#endif
}

void board_init(void) {
    uint8_t raw[BOARD_SERIAL_EFUSE_BYTES] = { 0 };
    board_load_raw(raw);
    cached = board_decode(raw);
    board_decode_serial(raw, serial_cache);
}

#if !defined(ESP32)
// The native stub has no eFuse, so tests inject a serial through this seam.
// It is cleared by board_init() (a zeroed raw region decodes to no serial).
void board_set_serial_for_test(const char *serial) {
    if (serial == NULL || serial[0] == '\0') {
        serial_cache[0] = '\0';
        return;
    }
    strncpy(serial_cache, serial, BOARD_SERIAL_BYTES);
    serial_cache[BOARD_SERIAL_BYTES] = '\0';
}
#endif

BoardId board_id(void) {
    return cached;
}

board_type_t board_type(void) {
    return cached.type;
}

const char *board_serial(void) {
    return serial_cache;
}

bool board_serial_present(void) {
    return serial_cache[0] != '\0';
}