#ifndef DRIFT_NET_H
#define DRIFT_NET_H

#include <Arduino.h>

// Network seam for the softAP dash: net_init() brings up WiFi, mDNS, the
// HTTP/OTA server and the status WebSocket; net_broadcast() pushes a status
// update to connected clients. The implementation (net.cpp) either runs the
// full stack (ESP32 target) or discards everything (e2e simulation and native
// tests, where the radio does not exist).

void net_init();
void net_broadcast(const String &status);

#endif
