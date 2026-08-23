#include <string.h>

#include "wifi_frame.h"

size_t wifi_frame_build_vendor_ie(uint8_t msg_counter, const uint8_t *pack, size_t pack_len, uint8_t *out_buf, size_t buf_size) {
    if (pack_len == 0 || pack_len > DRI_PACK_MAX_SIZE)
        return 0;
    if (buf_size < pack_len + WIFI_IE_HEADER_SIZE)
        return 0;

    out_buf[0] = WIFI_IE_ELEMENT_ID;
    out_buf[1] = (uint8_t)(pack_len + 5);  // IE length: OUI (3) + OUI type + counter + pack
    out_buf[2] = 0xFA;                     // ASD-STAN OUI FA:0B:BC
    out_buf[3] = 0x0B;
    out_buf[4] = 0xBC;
    out_buf[5] = WIFI_IE_ODID_OUI_TYPE;
    out_buf[6] = msg_counter;
    memcpy(&out_buf[7], pack, pack_len);
    return pack_len + WIFI_IE_HEADER_SIZE;
}
