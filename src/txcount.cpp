#include <string.h>

#include "txcount.h"

#define TXCOUNT_WINDOW_MS 1000UL

// Message Pack wire format (see txcount.h): 3-byte header + 25-byte messages.
#define TXCOUNT_PACK_HEADER_SIZE 3
#define TXCOUNT_MESSAGE_SIZE 25

static bool bt5_on = false, beacon_on = false, nan_on = false;

// Cumulative since txcount_init(); the window snapshot subtracts from these.
static TxRates total;
static TxRates window;              // last completed window, per second
static TxRates total_at_window_open;
static unsigned long window_open_ms = 0;

static uint32_t pack_messages(size_t pack_len) {
    if (pack_len < TXCOUNT_PACK_HEADER_SIZE + TXCOUNT_MESSAGE_SIZE)
        return 0;
    return (uint32_t)((pack_len - TXCOUNT_PACK_HEADER_SIZE) / TXCOUNT_MESSAGE_SIZE);
}

static uint32_t per_second(uint32_t delta, unsigned long elapsed_ms) {
    if (elapsed_ms == 0)
        return delta;
    // Round to nearest: windows drift a few ms either side of a second.
    return (uint32_t)(((uint64_t)delta * 1000 + elapsed_ms / 2) / elapsed_ms);
}

void txcount_init(bool bt5_enabled, bool beacon_enabled, bool nan_enabled) {
    bt5_on = bt5_enabled;
    beacon_on = beacon_enabled;
    nan_on = nan_enabled;
    memset(&total, 0, sizeof(total));
    memset(&window, 0, sizeof(window));
    memset(&total_at_window_open, 0, sizeof(total_at_window_open));
    window_open_ms = 0;
}

void txcount_bt4(void) {
    total.bt4_frames++;
    total.bt4_messages++;
}

void txcount_bt5(size_t pack_len) {
    if (!bt5_on)
        return;
    total.bt5_frames++;
    total.bt5_messages += pack_messages(pack_len);
}

void txcount_beacon(size_t pack_len) {
    if (!beacon_on)
        return;
    total.wifi_beacon_frames++;
    total.wifi_beacon_messages += pack_messages(pack_len);
}

void txcount_nan(size_t pack_len) {
    if (!nan_on)
        return;
    total.wifi_nan_frames++;
    total.wifi_nan_messages += pack_messages(pack_len);
}

void txcount_sample(unsigned long now) {
    unsigned long elapsed = now - window_open_ms;
    if (elapsed < TXCOUNT_WINDOW_MS)
        return;
    window.bt4_frames = per_second(total.bt4_frames - total_at_window_open.bt4_frames, elapsed);
    window.bt4_messages = per_second(total.bt4_messages - total_at_window_open.bt4_messages, elapsed);
    window.bt5_frames = per_second(total.bt5_frames - total_at_window_open.bt5_frames, elapsed);
    window.bt5_messages = per_second(total.bt5_messages - total_at_window_open.bt5_messages, elapsed);
    window.wifi_beacon_frames = per_second(total.wifi_beacon_frames - total_at_window_open.wifi_beacon_frames, elapsed);
    window.wifi_beacon_messages = per_second(total.wifi_beacon_messages - total_at_window_open.wifi_beacon_messages, elapsed);
    window.wifi_nan_frames = per_second(total.wifi_nan_frames - total_at_window_open.wifi_nan_frames, elapsed);
    window.wifi_nan_messages = per_second(total.wifi_nan_messages - total_at_window_open.wifi_nan_messages, elapsed);
    total_at_window_open = total;
    window_open_ms = now;
}

void txcount_rates(TxRates *out) {
    *out = window;
}
