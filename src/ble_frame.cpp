#include <string.h>

#include "ble_frame.h"

size_t ble_build_adv_frame(uint8_t msg_counter, const ODID_Message_encoded *enc, BleAdvFrame *out) {
    out->uuid = DRI_UUID;
    out->len = dri_build_service_data(msg_counter, enc, out->data);
    return out->len;
}

size_t ble_build_bt4_ad(uint8_t msg_counter, const ODID_Message_encoded *enc, uint8_t *out_buf, size_t buf_size) {
    if (buf_size < BLE_BT4_AD_SIZE)
        return 0;
    BleAdvFrame frame;
    ble_build_adv_frame(msg_counter, enc, &frame);

    out_buf[0] = (uint8_t)(frame.len + 3);        // AD length: type + UUID + service data
    out_buf[1] = BLE_AD_TYPE_SERVICE_DATA;
    out_buf[2] = (uint8_t)(frame.uuid & 0xFF);    // 16-bit service UUID, little-endian
    out_buf[3] = (uint8_t)(frame.uuid >> 8);
    memcpy(&out_buf[4], frame.data, frame.len);
    return 4 + frame.len;
}

size_t ble_build_bt5_ad(uint8_t msg_counter, const uint8_t *pack, size_t pack_len, uint8_t *out_buf, size_t buf_size) {
    if (pack_len == 0 || pack_len > DRI_PACK_MAX_SIZE)
        return 0;
    if (buf_size < pack_len + 6)
        return 0;

    out_buf[0] = (uint8_t)(pack_len + 5);         // AD length: type + UUID + app code + counter + pack
    out_buf[1] = BLE_AD_TYPE_SERVICE_DATA;
    out_buf[2] = (uint8_t)(DRI_UUID & 0xFF);
    out_buf[3] = (uint8_t)(DRI_UUID >> 8);
    size_t service_len = dri_build_pack_service_data(msg_counter, pack, pack_len, &out_buf[4]);
    return 4 + service_len;
}
