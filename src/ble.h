#ifndef DRIFT_BLE_H
#define DRIFT_BLE_H

#include <opendroneid.h>

// BLE transport seam for the Remote ID announcements. dri.cpp drives the
// broadcast schedule and calls ble_send() for every slot; the implementation
// (ble.cpp) either advertises over BLE (ESP32 target) or discards the message
// (e2e simulation and native tests, where the radio does not exist).
// The advertised frame itself (service UUID, app code, payload) is the
// host-testable seam ble_frame.h, pinned by the native tests.

void ble_init(const char *device_name);
void ble_send(uint8_t msg_counter, const ODID_Message_encoded *enc);

#if !defined(ESP32)
// Native test stub instrumentation (ble.cpp): a log of every ble_send(),
// so tests can assert on what the broadcast schedule actually emitted.
extern uint8_t ble_send_count;
extern uint8_t ble_send_counters[64];
extern ODID_Message_encoded ble_send_messages[64];
void ble_send_reset();
#endif

#endif
