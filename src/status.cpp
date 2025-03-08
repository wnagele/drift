#include <ArduinoJson.h>

#include "status.h"

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
