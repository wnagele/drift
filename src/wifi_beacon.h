#ifndef DRIFT_WIFI_BEACON_H
#define DRIFT_WIFI_BEACON_H

#include <opendroneid.h>

// Wi-Fi Beacon transport seam for the Remote ID announcements. dri.cpp drives
// the broadcast schedule and calls wifi_beacon_send_pack() for every Wi-Fi
// Beacon refresh (~5 Hz); the implementation (wifi_beacon.cpp) attaches the vendor IE
// built by wifi_frame.h to the SoftAP's beacons and probe responses on the
// device, or discards the message (e2e simulation and native tests, where the
// radio does not exist). The IE bytes themselves are the host-testable seam
// wifi_frame.h, pinned by the native tests.

// Wi-Fi Beacon transport (vendor IE on the SoftAP beacons). wifi_beacon_init()
// gates the transport; until it is called with true, wifi_beacon_send_pack()
// is a no-op.
void wifi_beacon_init(bool enabled);
void wifi_beacon_send_pack(uint8_t msg_counter, const uint8_t *pack, size_t pack_len);

#if !defined(ESP32)
// Native test stub instrumentation (wifi_beacon.cpp): a log of every
// wifi_beacon_send_pack(), so tests can assert on what the broadcast schedule
// actually emitted. wifi_beacon_send_reset() clears the record.
#include <stddef.h>
#include "dri.h"
extern uint8_t wifi_beacon_send_count;
extern uint8_t wifi_beacon_send_counters[64];
extern uint8_t wifi_beacon_send_lens[64];
extern uint8_t wifi_beacon_send_bytes[64][DRI_PACK_MAX_SIZE];
void wifi_beacon_send_reset();
#endif

#endif
