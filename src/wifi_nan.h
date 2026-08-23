#ifndef DRIFT_WIFI_NAN_H
#define DRIFT_WIFI_NAN_H

#include <stddef.h>
#include <stdint.h>

#include "dri.h"

// Wi-Fi NAN transport seam for the Remote ID announcements. The ESP32-C3 has
// no native NAN support (SOC_WIFI_NAN_SUPPORT is absent from its soc_caps.h;
// the esp_wifi_nan_* API only exists on ESP32/S2/C5/C61), so the transport is
// built the way ArduRemoteID does it: complete NAN frames from the vendored
// opendroneid builders, injected as raw 802.11 management frames with
// esp_wifi_80211_tx() on the SoftAP interface - which is already pinned to
// channel 6 (wifi_ap.h), the NAN cluster channel. dri.cpp drives the ~512 ms
// discovery-window cadence and builds the frames; this module owns the
// enabled gate, the source MAC and the radio. The frames themselves come
// from the vendored library and are pinned byte-exact by the native tests.

// Wi-Fi NAN frame layout: a fixed 51-byte action-frame tail (802.11 mgmt
// header, NAN service discovery header, service descriptor attribute,
// message counter byte, service descriptor extension attribute) around a
// full message pack; the smaller sync beacon fits the same buffer.
#define WIFI_NAN_FRAME_FIXED_SIZE 51
#define WIFI_NAN_FRAME_MAX_SIZE (WIFI_NAN_FRAME_FIXED_SIZE + DRI_PACK_MAX_SIZE)

void wifi_nan_init(bool enabled, const uint8_t mac[6]);
// The source MAC the frame builders (dri.cpp) embed - the eFuse base MAC,
// injected at init so the native tests can pin the frames against a known
// address.
const uint8_t *wifi_nan_mac();
// Transmit one complete NAN frame (sync beacon or action frame) built by the
// vendored encoder. wifi_nan_init() gates the transport; until it is called
// with true, both are no-ops.
void wifi_nan_send_sync_beacon(const uint8_t *frame, size_t len);
void wifi_nan_send_action_frame(const uint8_t *frame, size_t len);

#if !defined(ESP32)
// Native test stub instrumentation (wifi_nan.cpp): a log of every
// wifi_nan_send_*() call, so tests can assert on what the broadcast schedule
// actually emitted. wifi_nan_send_reset() clears both records.
extern uint8_t wifi_nan_sync_send_count;
extern uint8_t wifi_nan_sync_send_lens[64];
extern uint8_t wifi_nan_sync_send_bytes[64][WIFI_NAN_FRAME_MAX_SIZE];
extern uint8_t wifi_nan_action_send_count;
extern uint8_t wifi_nan_action_send_lens[64];
extern uint8_t wifi_nan_action_send_bytes[64][WIFI_NAN_FRAME_MAX_SIZE];
void wifi_nan_send_reset();
#endif

#endif
