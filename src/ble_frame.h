#ifndef DRIFT_BLE_FRAME_H
#define DRIFT_BLE_FRAME_H

#include <stddef.h>
#include <stdint.h>
#include <opendroneid.h>

#include "dri.h"

// Host-testable seam for the over-the-air BLE advertisements. ble.cpp reduces
// to translating these frames into the NimBLE API (plus the radio setup), so
// the on-air constants - the ASTM service UUID, the app code, the counter,
// the payload and the advertising interval - are pinned by the native tests
// instead of living only in the ESP32-only body.

// BLE advertising intervals are counted in 0.625 ms slots. The BT4
// announcement advertises at the broadcast schedule's slot cadence (each
// payload exactly once), and 100 ms is the Core Spec minimum for
// ADV_NONCONN_IND non-connectable advertising (Vol 4, Part E, 7.8.5).
#define BLE_ADV_INTERVAL ((uint16_t)(DRI_SLOT_INTERVAL / 0.625))

// The BLE5 Long Range instance repeats the message pack inside the
// DRI_PACK_INTERVAL window (same 0.625 ms slots).
#define BLE5_ADV_INTERVAL_MAX ((uint32_t)(DRI_PACK_INTERVAL / 0.625))
#define BLE5_ADV_INTERVAL_MIN ((uint32_t)(BLE5_ADV_INTERVAL_MAX * 0.75))

struct BleAdvFrame {
    uint16_t uuid;                        // service data UUID (ASTM 0xFFFA)
    uint8_t data[ODID_MESSAGE_SIZE + 2];  // app code, counter, ODID message
    size_t len;                           // bytes in data
};

size_t ble_build_adv_frame(uint8_t msg_counter, const ODID_Message_encoded *enc, BleAdvFrame *out);

// Raw advertising data: complete "Service Data - 16-bit UUID" AD structures
// ([length][0x16][UUID little-endian][service data]) as they go on air. The
// NimBLE path broadcasts these verbatim (setData / ble_gap_ext_adv_set_data),
// so their bytes are pinned natively rather than being assembled inside the
// ESP32-only body.

#define BLE_AD_TYPE_SERVICE_DATA 0x16

// BT4 legacy advertisement: 31-byte legacy PDU payload.
#define BLE_BT4_AD_SIZE (ODID_MESSAGE_SIZE + 2 + 4)
// BT5 Long Range advertisement: the message pack as service data.
#define BLE_BT5_AD_SIZE (DRI_PACK_MAX_SIZE + 2 + 4)

size_t ble_build_bt4_ad(uint8_t msg_counter, const ODID_Message_encoded *enc, uint8_t *out_buf, size_t buf_size);
size_t ble_build_bt5_ad(uint8_t msg_counter, const uint8_t *pack, size_t pack_len, uint8_t *out_buf, size_t buf_size);

#endif
