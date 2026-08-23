#ifndef DRIFT_BLE_H
#define DRIFT_BLE_H

#include <opendroneid.h>

// BLE transport seam for the Remote ID announcements. dri.cpp drives the
// broadcast schedule and calls ble_send() for every BT4 slot and
// ble_send_pack() for every BLE5 Long Range message pack; the implementation
// (ble.cpp) either advertises over BLE (ESP32 target) or discards the message
// (e2e simulation and native tests, where the radio does not exist).
// The advertised frames themselves (service UUID, app code, payload) are the
// host-testable seams ble_frame.h / dri.h, pinned by the native tests.

void ble_init(const char *device_name);
void ble_send(uint8_t msg_counter, const ODID_Message_encoded *enc);

// BLE5 Long Range transport (Coded PHY, message packs). ble5_init() gates the
// transport; until it is called with true, ble_send_pack() is a no-op.
void ble5_init(bool enabled);
void ble_send_pack(uint8_t msg_counter, const uint8_t *pack, size_t pack_len);

#if !defined(ESP32)
// Native test stub instrumentation (ble.cpp): a log of every ble_send() and
// ble_send_pack(), so tests can assert on what the broadcast schedule
// actually emitted. ble_send_reset() clears both records.
#include <stddef.h>
#include "dri.h"
extern uint8_t ble_send_count;
extern uint8_t ble_send_counters[64];
extern ODID_Message_encoded ble_send_messages[64];
extern uint8_t ble_pack_send_count;
extern uint8_t ble_pack_send_counters[64];
extern uint8_t ble_pack_send_lens[64];
extern uint8_t ble_pack_send_bytes[64][DRI_PACK_MAX_SIZE];
void ble_send_reset();
#endif

#endif
