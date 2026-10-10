#include "board.h"

#if defined(ESP32)
#include "esp_efuse.h"
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

#if defined(ESP32)

void board_init(void) {
    uint8_t raw[BOARD_EFUSE_BYTES] = { 0, 0, 0 };
    if (esp_efuse_read_block(EFUSE_BLK_USER_DATA, raw, 0, BOARD_EFUSE_BITS) != ESP_OK)
        return;  // leave the zeroed id: UNDEFINED rather than half-read
    cached = board_decode(raw);
}

#else // native tests: no eFuse, so no identity

void board_init(void) {
    cached = (BoardId){ BOARD_TYPE_UNDEFINED, 0, 0 };
}

#endif

BoardId board_id(void) {
    return cached;
}

board_type_t board_type(void) {
    return cached.type;
}