#include <esp32-hal.h>
#include <BLEDevice.h>
#include "dri.h"

#define DRI_INTERVAL 20         // ms
#define DRI_GUARD_MULTIPLIER 2  // ensure there is sufficient time to broadcast

#define DRI_UUID 0xFFFA         // ASTM
#define DRI_APP_CODE 0x0D       // RD

ODID_Message_encoded encoded;
uint8_t counter = 0;
uint8_t schedule_counter = 0;
unsigned long last;

void ble_transmit(ODID_Message_encoded encoded) {
        uint8_t buf[ODID_MESSAGE_SIZE + 2];
        memset(&buf, 0, sizeof(buf));
        buf[0] = (uint8_t)DRI_APP_CODE;
        buf[1] = counter++;
        for (int i = 0; i < ODID_MESSAGE_SIZE; i++)
            buf[2 + i] = encoded.rawData[i];

        BLEAdvertisementData adv_data;
        adv_data.setServiceData(BLEUUID((uint16_t)DRI_UUID), std::string((char*)buf, sizeof(buf)));

        BLEAdvertising *adv = BLEDevice::getAdvertising();
        adv->setAdvertisementData(adv_data);
}

void dri_init(ODID_UAS_Data *data) {
    odid_initUasData(data);

    data->BasicID[0].UAType = ODID_UATYPE_HELICOPTER_OR_MULTIROTOR;
    data->BasicID[0].IDType = ODID_IDTYPE_SERIAL_NUMBER;
    strncpy(data->BasicID[0].UASID, "12345", ODID_ID_SIZE);

    data->SelfID.DescType = ODID_DESC_TYPE_TEXT;
    strncpy(data->SelfID.Desc, "test", ODID_STR_SIZE);

    data->OperatorID.OperatorIdType = ODID_OPERATOR_ID;
    strncpy(data->OperatorID.OperatorId, "424242", ODID_ID_SIZE);

    data->System.ClassificationType = ODID_CLASSIFICATION_TYPE_EU;
    data->System.CategoryEU = ODID_CATEGORY_EU_OPEN;
    data->System.ClassEU = ODID_CLASS_EU_CLASS_0;

    /*
    data->System.AreaCount = 1;
    data->System.AreaRadius = 500;
    data->System.AreaFloor = 0;
    data->System.AreaCeiling = 50;
    */

    BLEDevice::init("test");
    BLEAdvertising *adv = BLEDevice::getAdvertising();
    adv->setMinInterval(DRI_INTERVAL / 0.625);
    adv->setMaxInterval(DRI_INTERVAL / 0.625);
    adv->start();

    last = millis();
}

void dri_transmit(ODID_UAS_Data *data) {
    unsigned long now = millis();
    if (now - last > DRI_INTERVAL * DRI_GUARD_MULTIPLIER) {
        last = now;

        switch (++schedule_counter) {
            case 1:
                memset(&encoded, 0, sizeof(ODID_Message_encoded));
                encodeBasicIDMessage((ODID_BasicID_encoded*) &encoded, &data->BasicID[0]);
                ble_transmit(encoded);
                break;
            case 2:
                memset(&encoded, 0, sizeof(ODID_Message_encoded));
                encodeSelfIDMessage((ODID_SelfID_encoded*) &encoded, &data->SelfID);
                ble_transmit(encoded);
                break;
            case 3:
                memset(&encoded, 0, sizeof(ODID_Message_encoded));
                encodeOperatorIDMessage((ODID_OperatorID_encoded*) &encoded, &data->OperatorID);
                ble_transmit(encoded);
                break;
            case 4:
                memset(&encoded, 0, sizeof(ODID_Message_encoded));
                encodeSystemMessage((ODID_System_encoded*) &encoded, &data->System);
                ble_transmit(encoded);
                break;
            default:
                memset(&encoded, 0, sizeof(ODID_Message_encoded));
                encodeLocationMessage((ODID_Location_encoded*) &encoded, &data->Location);
                ble_transmit(encoded);
                break;
        }
        if (schedule_counter >= (1000 / DRI_INTERVAL / DRI_GUARD_MULTIPLIER))
            schedule_counter = 0;

        /*
        TODO - should we send all of these?

        for (uint8_t i = 0; i < ODID_BASIC_ID_MAX_MESSAGES; i++) {
            memset(&encoded, 0, sizeof(ODID_Message_encoded));
            encodeBasicIDMessage((ODID_BasicID_encoded*) &encoded, &data->BasicID[i]);
            ble_transmit(encoded);
        }

        memset(&encoded, 0, sizeof(ODID_Message_encoded));
        encodeLocationMessage((ODID_Location_encoded*) &encoded, &data->Location);
        ble_transmit(encoded);

        for (uint8_t i = 0; i < ODID_AUTH_MAX_PAGES; i++) {
            memset(&encoded, 0, sizeof(ODID_Message_encoded));
            encodeAuthMessage((ODID_Auth_encoded*) &encoded, &data->Auth[i]);
            ble_transmit(encoded);
        }

        memset(&encoded, 0, sizeof(ODID_Message_encoded));
        encodeSelfIDMessage((ODID_SelfID_encoded*) &encoded, &data->SelfID);
        ble_transmit(encoded);

        memset(&encoded, 0, sizeof(ODID_Message_encoded));
        encodeOperatorIDMessage((ODID_OperatorID_encoded*) &encoded, &data->OperatorID);
        ble_transmit(encoded);

        memset(&encoded, 0, sizeof(ODID_Message_encoded));
        encodeSystemMessage((ODID_System_encoded*) &encoded, &data->System);
        ble_transmit(encoded);
        */
    }
}

void dri_update_status(ODID_UAS_Data *data, ODID_status_t status) {
    data->Location.Status = status;
}

void dri_update_location(
    ODID_UAS_Data *data,
    double lat,
    double lon,
    double alt,
    double rel_alt,
    float hdg
) {
    data->Location.Latitude = lat;
    data->Location.Longitude = lon;
    data->Location.AltitudeGeo = alt;
    data->Location.HeightType = ODID_HEIGHT_REF_OVER_TAKEOFF;
    data->Location.Height = rel_alt;
    data->Location.Direction = hdg;
}

void dri_update_operator(
    ODID_UAS_Data *data,
    double lat,
    double lon,
    float alt
) {
    data->System.OperatorLocationType = ODID_OPERATOR_LOCATION_TYPE_TAKEOFF;
    data->System.OperatorLatitude = lat;
    data->System.OperatorLongitude = lon;
    data->System.OperatorAltitudeGeo = alt;
}