#ifndef DRIFT_BLE_FRAME_H
#define DRIFT_BLE_FRAME_H

#include <stddef.h>
#include <stdint.h>
#include <opendroneid.h>

#include "dri.h"

// Host-testable seam for the over-the-air BLE advertisement. ble.cpp reduces
// to translating this frame into the ArduinoBLE API (plus the radio setup),
// so the on-air constants - the ASTM service UUID, the app code, the counter,
// the payload and the advertising interval - are pinned by the native tests
// instead of living only in the ESP32-only body.

// BLE advertising intervals are counted in 0.625 ms slots; the announcements
// keep the pace of the broadcast schedule (DRI_INTERVAL).
#define BLE_ADV_INTERVAL ((uint16_t)(DRI_INTERVAL / 0.625))

struct BleAdvFrame {
    uint16_t uuid;                        // service data UUID (ASTM 0xFFFA)
    uint8_t data[ODID_MESSAGE_SIZE + 2];  // app code, counter, ODID message
    size_t len;                           // bytes in data
};

size_t ble_build_adv_frame(uint8_t msg_counter, const ODID_Message_encoded *enc, BleAdvFrame *out);

#endif
