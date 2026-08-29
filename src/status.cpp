#include <ArduinoJson.h>

#include "status.h"
#include "txcount.h"

bool telemetry = false, gnss = false;
uint32_t telemetry_count = 0, gnss_count = 0;

void status_process() {
    telemetry = telemetry_count > 0;
    gnss = gnss_count > 0;
    telemetry_count = 0;
    gnss_count = 0;
}

String status_get() {
    JsonDocument doc;
    doc["type"] = "status";
    doc["telemetry"] = telemetry;
    doc["gnss"] = gnss;

    // Transmit rates from the last completed one-second window (txcount.cpp):
    // frames the broadcast schedule handed to the radio seams and the ODID
    // messages they carried, per transport. Key names mirror the config API
    // fields (bt5_enabled, wifi_beacon_enabled, wifi_nan_enabled).
    TxRates tx;
    txcount_rates(&tx);
    doc["tx"]["bt4"]["frames"] = tx.bt4_frames;
    doc["tx"]["bt4"]["messages"] = tx.bt4_messages;
    doc["tx"]["bt5"]["frames"] = tx.bt5_frames;
    doc["tx"]["bt5"]["messages"] = tx.bt5_messages;
    doc["tx"]["wifi_beacon"]["frames"] = tx.wifi_beacon_frames;
    doc["tx"]["wifi_beacon"]["messages"] = tx.wifi_beacon_messages;
    doc["tx"]["wifi_nan"]["frames"] = tx.wifi_nan_frames;
    doc["tx"]["wifi_nan"]["messages"] = tx.wifi_nan_messages;

    String buf;
    serializeJson(doc, buf);
    return buf;
}

void status_telemetry_rcvd() {
    telemetry_count++;
}

void status_gnss_rcvd() {
    gnss_count++;
}
