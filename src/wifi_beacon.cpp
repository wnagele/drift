#include <Arduino.h>

#include "wifi_beacon.h"
#include "wifi_frame.h"

#if defined(ESP32) && !defined(DRIFT_NO_NET)

#include <esp_wifi.h>

static bool wifi_beacon_enabled = false;

// The vendor IE rides on the frames the SoftAP already transmits: registered
// once for the beacons (WIFI_VND_IE_TYPE_BEACON) and once for the probe
// responses (WIFI_VND_IE_TYPE_PROBE_RESP) - the probe-response mirror is the
// known fix for poor Android reception, where phones otherwise only see the
// IE when their scan happens to align with a beacon. The SoftAP is pinned to
// channel 6 (wifi_ap.h) so receivers know where to listen.
//
// The driver rejects setting a vendor IE at an index that is already enabled
// (ESP_ERR_INVALID_ARG), so every refresh is a clear-then-set for both frame
// types - the same dance production Remote ID modules use. Failures are
// simply retried on the next refresh; between the clears and the sets a
// beacon may go out without the IE, which receivers treat as a lost frame
// (the counter in the IE lets them detect the gap).
//
// The vendor_ie_data_t the driver expects is a flexible-array struct: its
// payload must follow the 6-byte header inline. wifi_frame_build_vendor_ie()
// produces exactly that byte layout, so its buffer is cast whole instead of
// assembling a struct with a payload pointer (the flexible-array pitfall
// behind ArduRemoteID issue #155).

void wifi_beacon_init(bool enabled) {
    wifi_beacon_enabled = enabled;
}

void wifi_beacon_send_pack(uint8_t msg_counter, const uint8_t *pack, size_t pack_len) {
    if (!wifi_beacon_enabled)
        return;

    uint8_t ie[WIFI_IE_MAX_SIZE];
    size_t len = wifi_frame_build_vendor_ie(msg_counter, pack, pack_len, ie, sizeof(ie));
    if (len == 0)
        return;

    esp_wifi_set_vendor_ie(false, WIFI_VND_IE_TYPE_BEACON, WIFI_VND_IE_ID_0, NULL);
    esp_wifi_set_vendor_ie(false, WIFI_VND_IE_TYPE_PROBE_RESP, WIFI_VND_IE_ID_0, NULL);
    esp_wifi_set_vendor_ie(true, WIFI_VND_IE_TYPE_BEACON, WIFI_VND_IE_ID_0, (vendor_ie_data_t *)ie);
    esp_wifi_set_vendor_ie(true, WIFI_VND_IE_TYPE_PROBE_RESP, WIFI_VND_IE_ID_0, (vendor_ie_data_t *)ie);
}

#elif defined(ESP32)

// e2e simulation (DRIFT_NO_NET): no radio. The gdb harness reads
// wifi_beacon_send_pack()'s arguments at the stub's entry (see
// test/e2e/harness.py), so the bodies must stay empty.
void wifi_beacon_init(bool) {}
void wifi_beacon_send_pack(uint8_t, const uint8_t *, size_t) {}

#else // native tests: record every send so tests can assert on what the
      // broadcast schedule actually emitted

uint8_t wifi_beacon_send_count = 0;
uint8_t wifi_beacon_send_counters[64];
uint8_t wifi_beacon_send_lens[64];
uint8_t wifi_beacon_send_bytes[64][DRI_PACK_MAX_SIZE];

void wifi_beacon_send_reset() {
    wifi_beacon_send_count = 0;
}

void wifi_beacon_init(bool) {}

void wifi_beacon_send_pack(uint8_t msg_counter, const uint8_t *pack, size_t pack_len) {
    if (wifi_beacon_send_count < sizeof(wifi_beacon_send_counters)) {
        wifi_beacon_send_counters[wifi_beacon_send_count] = msg_counter;
        wifi_beacon_send_lens[wifi_beacon_send_count] = (uint8_t)pack_len;
        if (pack_len > DRI_PACK_MAX_SIZE)
            pack_len = DRI_PACK_MAX_SIZE;
        memcpy(wifi_beacon_send_bytes[wifi_beacon_send_count], pack, pack_len);
    }
    wifi_beacon_send_count++;
}

#endif
