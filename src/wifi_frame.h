#ifndef DRIFT_WIFI_FRAME_H
#define DRIFT_WIFI_FRAME_H

#include <stddef.h>
#include <stdint.h>
#include <opendroneid.h>

#include "dri.h"

// Host-testable seam for the over-the-air Wi-Fi Beacon bytes. wifi_beacon.cpp
// reduces to registering this Information Element with the WiFi driver (plus
// the clear-before-set dance the driver requires), so the on-air constants -
// the vendor IE element id, the ASD-STAN OUI, the app code, the counter and
// the message pack payload - are pinned by the native tests instead of living
// only in the ESP32-only body.

// Vendor-Specific Information Element (IEEE 802.11 element id 221).
#define WIFI_IE_ELEMENT_ID 0xDD

// ASD-STAN Remote ID OUI FA:0B:BC with OUI type 0x0D ("RD") - the Wi-Fi
// sibling of the ASTM BLE service data header (DRI_APP_CODE).
#define WIFI_IE_ODID_OUI_TYPE 0x0D

// element id + length + OUI (3) + OUI type + message counter.
#define WIFI_IE_HEADER_SIZE 7
#define WIFI_IE_MAX_SIZE (WIFI_IE_HEADER_SIZE + DRI_PACK_MAX_SIZE)

size_t wifi_frame_build_vendor_ie(uint8_t msg_counter, const uint8_t *pack, size_t pack_len, uint8_t *out_buf, size_t buf_size);

#endif
