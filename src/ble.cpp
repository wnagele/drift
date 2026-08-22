#include <Arduino.h>

#include "ble.h"
#include "ble_frame.h"
#include "dri.h"

#if defined(ESP32) && !defined(DRIFT_NO_BLE)

#include <BLEDevice.h>

void ble_init(const char *device_name) {
    BLEDevice::init(device_name);
    BLEAdvertising *adv = BLEDevice::getAdvertising();
    adv->setMinInterval(BLE_ADV_INTERVAL);
    adv->setMaxInterval(BLE_ADV_INTERVAL);
    adv->start();
}

void ble_send(uint8_t msg_counter, const ODID_Message_encoded *enc) {
    BleAdvFrame frame;
    ble_build_adv_frame(msg_counter, enc, &frame);

    BLEAdvertisementData adv_data;
    // Arduino core 3.x switched the BLE payload type from std::string to
    // String. Both hold the length explicitly, so the NUL bytes in an ODID
    // message survive either way.
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    adv_data.setServiceData(BLEUUID(frame.uuid), String((char *)frame.data, frame.len));
#else
    adv_data.setServiceData(BLEUUID(frame.uuid), std::string((char *)frame.data, frame.len));
#endif

    BLEDevice::getAdvertising()->setAdvertisementData(adv_data);
}

#elif defined(ESP32)

// e2e simulation (DRIFT_NO_BLE): no radio. The gdb harness reads
// ble_send()'s arguments at the stub's entry (see test/e2e/harness.py), so
// the body must stay empty.
void ble_init(const char *) {}
void ble_send(uint8_t, const ODID_Message_encoded *) {}

#else // native tests: record every send so tests can assert on what the
      // broadcast schedule actually emitted

uint8_t ble_send_count = 0;
uint8_t ble_send_counters[64];
ODID_Message_encoded ble_send_messages[64];

void ble_send_reset() {
    ble_send_count = 0;
}

void ble_init(const char *) {}

void ble_send(uint8_t msg_counter, const ODID_Message_encoded *enc) {
    if (ble_send_count < sizeof(ble_send_counters)) {
        ble_send_counters[ble_send_count] = msg_counter;
        ble_send_messages[ble_send_count] = *enc;
    }
    ble_send_count++;
}

#endif
