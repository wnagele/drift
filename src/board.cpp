#include "board.h"

#include <string.h>

#if defined(ESP32)
#include "esp_efuse.h"
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

#if defined(ESP32)

void board_init(void) {
    uint8_t raw[BOARD_SERIAL_EFUSE_BYTES] = { 0 };
    if (esp_efuse_read_block(EFUSE_BLK_USER_DATA, raw, 0,
                             BOARD_SERIAL_EFUSE_BITS) != ESP_OK) {
        // Leave the zeroed caches: UNDEFINED and no serial rather than
        // half-read values.
        return;
    }
    cached = board_decode(raw);
    board_decode_serial(raw, serial_cache);
}

#else // native tests: no eFuse, so no identity and no serial

void board_init(void) {
    cached = (BoardId){ BOARD_TYPE_UNDEFINED, 0, 0 };
    serial_cache[0] = '\0';
}

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