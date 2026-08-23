#include <Arduino.h>

#include "ble.h"
#include "ble_frame.h"
#include "dri.h"

#if defined(ESP32) && !defined(DRIFT_NO_BLE)

#include <NimBLEDevice.h>
#include "nimble/nimble/host/include/host/ble_gap.h"
#include "nimble/porting/nimble/include/os/os_mbuf.h"

// Both DRIFT transports run as extended-advertising instances of the NimBLE
// host: the Bluetooth spec allows only one of the legacy and extended
// advertising enable commands to be in use, so the BT4 announcement is a
// legacy PDU (ADV_NONCONN_IND, spec-correct for ODID) on instance 0 and the
// BLE5 Long Range message pack is a Coded-PHY advertisement on instance 1 -
// the architecture production Remote ID modules (ArduRemoteID) use. Both
// instances use the controller's public address, so receivers see one device
// behind the two transports.
//
// Advertising PDUs on the LE Coded PHY use S=8 coding (S=2 requires the
// optional coding-selection feature), i.e. instance 1 is Long Range S=8.

#define BLE_ADV_INSTANCE_BT4 0
#define BLE_ADV_INSTANCE_BT5 1

static NimBLEExtAdvertising *ext_adv = NULL;
static bool bt4_active = false;
static bool bt5_active = false;
static bool bt5_enabled = false;

// Configure and start an instance with its initial payload. Fails (returns
// false) while the BLE host has not synced yet - or after a host reset
// cleared the instance - in which case the caller simply retries on its next
// send; nothing is lost but a little latency at boot.
static bool adv_instance_start(uint8_t instance, NimBLEExtAdvertisement &ad) {
    if (!ext_adv->setInstanceData(instance, ad))
        return false;
    return ext_adv->start(instance);
}

// Replace the payload of a running instance. The NimBLE host allows this
// without stopping the instance: always for legacy PDUs, and for extended
// non-scannable instances while the payload fits a single HCI command (the
// BT5 payload is at most BLE_BT5_AD_SIZE = 234 bytes; the limit is 251).
// The host consumes the mbuf on every path.
static bool adv_instance_update(uint8_t instance, const uint8_t *ad, size_t len) {
    struct os_mbuf *buf = os_msys_get_pkthdr(len, 0);
    if (buf == NULL)
        return false;
    if (os_mbuf_append(buf, ad, len) != 0) {
        os_mbuf_free_chain(buf);
        return false;
    }
    return ble_gap_ext_adv_set_data(instance, buf) == 0;
}

void ble_init(const char *device_name) {
    NimBLEDevice::init(device_name);
    ext_adv = NimBLEDevice::getAdvertising();
}

void ble5_init(bool enabled) {
    bt5_enabled = enabled;
}

void ble_send(uint8_t msg_counter, const ODID_Message_encoded *enc) {
    if (ext_adv == NULL)
        return;

    uint8_t ad[BLE_BT4_AD_SIZE];
    size_t len = ble_build_bt4_ad(msg_counter, enc, ad, sizeof(ad));
    if (len == 0)
        return;

    if (!bt4_active) {
        NimBLEExtAdvertisement params;  // primary/secondary PHY 1M: legacy PDU
        params.setLegacyAdvertising(true);
        params.setConnectable(false);
        params.setScannable(false);
        params.setMinInterval(BLE_ADV_INTERVAL);
        params.setMaxInterval(BLE_ADV_INTERVAL);
        params.setData(ad, len);
        bt4_active = adv_instance_start(BLE_ADV_INSTANCE_BT4, params);
        return;
    }
    // A failed update (e.g. host reset cleared the instance) drops the
    // active flag so the next slot reconfigures the instance from scratch.
    if (!adv_instance_update(BLE_ADV_INSTANCE_BT4, ad, len))
        bt4_active = false;
}

void ble_send_pack(uint8_t msg_counter, const uint8_t *pack, size_t pack_len) {
    if (ext_adv == NULL || !bt5_enabled)
        return;

    uint8_t ad[BLE_BT5_AD_SIZE];
    size_t len = ble_build_bt5_ad(msg_counter, pack, pack_len, ad, sizeof(ad));
    if (len == 0)
        return;

    if (!bt5_active) {
        NimBLEExtAdvertisement params(BLE_HCI_LE_PHY_CODED, BLE_HCI_LE_PHY_CODED);
        params.setConnectable(false);
        params.setScannable(false);
        params.setMinInterval(BLE5_ADV_INTERVAL_MIN);
        params.setMaxInterval(BLE5_ADV_INTERVAL_MAX);
        params.setData(ad, len);
        bt5_active = adv_instance_start(BLE_ADV_INSTANCE_BT5, params);
        return;
    }
    if (!adv_instance_update(BLE_ADV_INSTANCE_BT5, ad, len))
        bt5_active = false;
}

#elif defined(ESP32)

// e2e simulation (DRIFT_NO_BLE): no radio. The gdb harness reads
// ble_send()'s and ble_send_pack()'s arguments at the stubs' entry (see
// test/e2e/harness.py), so the bodies must stay empty.
void ble_init(const char *) {}
void ble5_init(bool) {}
void ble_send(uint8_t, const ODID_Message_encoded *) {}
void ble_send_pack(uint8_t, const uint8_t *, size_t) {}

#else // native tests: record every send so tests can assert on what the
      // broadcast schedule actually emitted

uint8_t ble_send_count = 0;
uint8_t ble_send_counters[64];
ODID_Message_encoded ble_send_messages[64];

uint8_t ble_pack_send_count = 0;
uint8_t ble_pack_send_counters[64];
uint8_t ble_pack_send_lens[64];
uint8_t ble_pack_send_bytes[64][DRI_PACK_MAX_SIZE];

void ble_send_reset() {
    ble_send_count = 0;
    ble_pack_send_count = 0;
}

void ble_init(const char *) {}
void ble5_init(bool) {}

void ble_send(uint8_t msg_counter, const ODID_Message_encoded *enc) {
    if (ble_send_count < sizeof(ble_send_counters)) {
        ble_send_counters[ble_send_count] = msg_counter;
        ble_send_messages[ble_send_count] = *enc;
    }
    ble_send_count++;
}

void ble_send_pack(uint8_t msg_counter, const uint8_t *pack, size_t pack_len) {
    if (ble_pack_send_count < sizeof(ble_pack_send_counters)) {
        ble_pack_send_counters[ble_pack_send_count] = msg_counter;
        ble_pack_send_lens[ble_pack_send_count] = (uint8_t)pack_len;
        if (pack_len > DRI_PACK_MAX_SIZE)
            pack_len = DRI_PACK_MAX_SIZE;
        memcpy(ble_pack_send_bytes[ble_pack_send_count], pack, pack_len);
    }
    ble_pack_send_count++;
}

#endif
