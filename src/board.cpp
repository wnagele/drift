#include "board.h"

#include <string.h>

#if defined(ESP32)
#include "esp_efuse.h"
#endif

// Test-build identity overrides (see board.h): DRIFT_BOARD_OVERRIDE_TYPE is
// the anchor - defined, the identity is fabricated and eFuse is never read.
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
#endif

static BoardId cached = { BOARD_TYPE_UNDEFINED, 0, 0 };

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
static void board_load_raw(uint8_t raw[BOARD_EFUSE_BYTES]) {
#if defined(DRIFT_BOARD_OVERRIDE_TYPE)
    raw[0] = DRIFT_BOARD_OVERRIDE_TYPE;
    raw[1] = DRIFT_BOARD_OVERRIDE_REV_MAJOR;
    raw[2] = DRIFT_BOARD_OVERRIDE_REV_MINOR;
#elif defined(ESP32)
    // On a read error raw stays zeroed: UNDEFINED rather than half-read.
    (void)esp_efuse_read_block(EFUSE_BLK_USER_DATA, raw, 0, BOARD_EFUSE_BITS);
#else // native tests: no eFuse, so no identity - raw stays zeroed
    (void)raw;
#endif
}

void board_init(void) {
    uint8_t raw[BOARD_EFUSE_BYTES] = { 0, 0, 0 };
    board_load_raw(raw);
    cached = board_decode(raw);
}

BoardId board_id(void) {
    return cached;
}

board_type_t board_type(void) {
    return cached.type;
}