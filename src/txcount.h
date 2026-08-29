#ifndef DRIFT_TXCOUNT_H
#define DRIFT_TXCOUNT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Per-transport transmit diagnostics for the dash Status view: what the
// broadcast schedule handed to the radio seams over the last completed
// one-second window. Counting happens in dri.cpp at the send seams (where
// the pack lengths are known); this module owns the config enable gates (a
// disabled transport must not report phantom activity) and the rate
// sampling. Frames are Remote-ID-carrying emissions — one BT4 payload
// update, one pack advertisement, one vendor-IE refresh, one NAN action
// frame; a NAN window's sync beacon carries no ODID data and is not
// counted, so frames:messages is 1:N on every pack-carrying transport.
// Messages are the ODID messages those frames carry (a Message Pack holds
// N).
typedef struct {
    uint32_t bt4_frames, bt4_messages;
    uint32_t bt5_frames, bt5_messages;
    uint32_t wifi_beacon_frames, wifi_beacon_messages;
    uint32_t wifi_nan_frames, wifi_nan_messages;
} TxRates;

// Mirrors the config gates of the radio seams (main.cpp wires them from
// config_*_enabled()); also resets every counter and rate, so tests call
// it in setUp().
void txcount_init(bool bt5_enabled, bool beacon_enabled, bool nan_enabled);

// BT4 has no config gate — it counts unconditionally.
void txcount_bt4(void);

// The pack length carries its message count: a Message Pack is a 3-byte
// header (type/version, single-message size, message count) followed by
// 25-byte ODID messages (odid_wifi.c odid_message_build_pack).
void txcount_bt5(size_t pack_len);
void txcount_beacon(size_t pack_len);
void txcount_nan(size_t pack_len);

// Close the current window once at least a second has elapsed; rates are
// normalized to per-second, so a window that drifted (loop stall, first
// window after boot) still reports a true rate. Call from the main loop
// with millis(), like the dri_*_due guards — unsigned subtraction keeps
// the millis() wraparound safe.
void txcount_sample(unsigned long now);

// The last completed window (all zeros before the first one closes).
void txcount_rates(TxRates *out);

#endif
