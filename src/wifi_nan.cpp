#include <Arduino.h>
#include <string.h>

#include "wifi_nan.h"

#if defined(ESP32) && !defined(DRIFT_NO_NET)

#include <esp_wifi.h>

static bool wifi_nan_enabled = false;
static uint8_t wifi_nan_addr[6];

// Raw frame injection instead of a NAN stack: the C3 silicon has no NAN
// engine, so the sync beacon and the action frame are handed to the driver
// as complete 802.11 management frames via esp_wifi_80211_tx() on the AP
// interface (channel 6, the NAN cluster channel), with the driver filling in
// the sequence numbers (en_sys_seq) - the same approach ArduRemoteID's
// WiFi_TX uses on chips without esp_wifi_nan_* support. The frames go out on
// air alongside the SoftAP's own beacons; the injected frames are
// unencrypted management frames any NAN receiver in range can decode.
//
// The source MAC embedded in the frames is the eFuse base MAC (what
// ArduRemoteID puts in its ODID frames too), not the SoftAP's base+1
// address: the NAN service is its own entity keying the announcements, and
// the base MAC also matches the STA interface.
//
// A rejected esp_wifi_80211_tx() (e.g. before the AP interface came up) is
// simply dropped: the next discovery window retries with a fresh frame.

void wifi_nan_init(bool enabled, const uint8_t mac[6]) {
    wifi_nan_enabled = enabled;
    memcpy(wifi_nan_addr, mac, 6);
}

const uint8_t *wifi_nan_mac() {
    return wifi_nan_addr;
}

void wifi_nan_send_sync_beacon(const uint8_t *frame, size_t len) {
    if (!wifi_nan_enabled)
        return;
    esp_wifi_80211_tx(WIFI_IF_AP, frame, len, true);
}

void wifi_nan_send_action_frame(const uint8_t *frame, size_t len) {
    if (!wifi_nan_enabled)
        return;
    esp_wifi_80211_tx(WIFI_IF_AP, frame, len, true);
}

#elif defined(ESP32)

// e2e simulation (DRIFT_NO_NET): no radio. The gdb harness reads the send
// stubs' arguments at entry (see test/e2e/harness.py), so their bodies must
// stay empty; init still records the MAC the frame builders embed, which the
// broadcast scenario reads back out of the captured frames.
static uint8_t wifi_nan_addr[6];

void wifi_nan_init(bool, const uint8_t mac[6]) {
    memcpy(wifi_nan_addr, mac, 6);
}

const uint8_t *wifi_nan_mac() {
    return wifi_nan_addr;
}

void wifi_nan_send_sync_beacon(const uint8_t *, size_t) {}
void wifi_nan_send_action_frame(const uint8_t *, size_t) {}

#else // native tests: record every send so tests can assert on what the
      // broadcast schedule actually emitted

static uint8_t wifi_nan_addr[6];

uint8_t wifi_nan_sync_send_count = 0;
uint8_t wifi_nan_sync_send_lens[64];
uint8_t wifi_nan_sync_send_bytes[64][WIFI_NAN_FRAME_MAX_SIZE];

uint8_t wifi_nan_action_send_count = 0;
uint8_t wifi_nan_action_send_lens[64];
uint8_t wifi_nan_action_send_bytes[64][WIFI_NAN_FRAME_MAX_SIZE];

void wifi_nan_send_reset() {
    wifi_nan_sync_send_count = 0;
    wifi_nan_action_send_count = 0;
}

void wifi_nan_init(bool, const uint8_t mac[6]) {
    memcpy(wifi_nan_addr, mac, 6);
}

const uint8_t *wifi_nan_mac() {
    return wifi_nan_addr;
}

static void wifi_nan_record(uint8_t *count, uint8_t *lens, uint8_t bytes[][WIFI_NAN_FRAME_MAX_SIZE],
                            const uint8_t *frame, size_t len) {
    if (*count < 64) {
        lens[*count] = (uint8_t)len;
        if (len > WIFI_NAN_FRAME_MAX_SIZE)
            len = WIFI_NAN_FRAME_MAX_SIZE;
        memcpy(bytes[*count], frame, len);
    }
    (*count)++;
}

void wifi_nan_send_sync_beacon(const uint8_t *frame, size_t len) {
    wifi_nan_record(&wifi_nan_sync_send_count, wifi_nan_sync_send_lens,
                    wifi_nan_sync_send_bytes, frame, len);
}

void wifi_nan_send_action_frame(const uint8_t *frame, size_t len) {
    wifi_nan_record(&wifi_nan_action_send_count, wifi_nan_action_send_lens,
                    wifi_nan_action_send_bytes, frame, len);
}

#endif
