#include "ble_frame.h"

size_t ble_build_adv_frame(uint8_t msg_counter, const ODID_Message_encoded *enc, BleAdvFrame *out) {
    out->uuid = DRI_UUID;
    out->len = dri_build_service_data(msg_counter, enc, out->data);
    return out->len;
}
