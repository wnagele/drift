#include "net.h"

#if defined(ESP32) && !defined(DRIFT_NO_NET)

#include <WiFi.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncHTTPUpdateServer.h>

#include "config.h"
#include "dash.h"
#include "debug.h"
#include "http_api.h"
#include "wifi_ap.h"

static ESPAsyncHTTPUpdateServer update_server;
static AsyncWebServer server(80);
static AsyncWebSocket ws("/ws");

// Translate a route decision (http_api.cpp) into a server response.
static void send_response(AsyncWebServerRequest *request, const HttpApiResponse &route) {
    AsyncWebServerResponse *response;
    if (route.body != NULL)
        response = request->beginResponse(route.status, route.content_type, route.body, route.body_len);
    else
        response = request->beginResponse(route.status, route.content_type, route.body_text);
    if (route.gzip)
        response->addHeader("Content-Encoding", "gzip");
    request->send(response);
    if (route.restart_after) {
        delay(100);
        ESP.restart();
    }
}

void net_init() {
    // Open vs. WPA softAP is a config decision (wifi_ap.h, natively tested);
    // this side only owns the WiFi calls themselves.
    WifiApParams ap = wifi_ap_params();
    // An empty passphrase keeps the network open (AP.cpp treats "" like NULL);
    // the channel is pinned by wifi_ap_params() (see WIFI_AP_CHANNEL).
    if (ap.secure)
        WiFi.softAP(ap.ssid, ap.password, ap.channel);
    else
        WiFi.softAP(ap.ssid, "", ap.channel);

    MDNS.begin("drift");
    MDNS.addService("http", "tcp", 80);

    update_server.setup(&server);

    http_api_init(DASH, sizeof(DASH));

    server.begin();
    server.addHandler(&ws);
    server.onNotFound([](AsyncWebServerRequest *request) { request->send(404); });
    server.on(HTTP_API_ROOT, HTTP_GET, [](AsyncWebServerRequest *request) {
        send_response(request, http_api_get(HTTP_API_ROOT));
    });
    server.on(HTTP_API_CONFIG, HTTP_GET, [](AsyncWebServerRequest *request) {
        send_response(request, http_api_get(HTTP_API_CONFIG));
    });
    server.on(HTTP_API_CONFIG, HTTP_POST, [](AsyncWebServerRequest *request) {
        // Reached only for a bodiless POST; requests with a body are
        // routed to the body handler below once the body has arrived.
        request->send(http_api_post_config(NULL).status);
    }, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        // The body buffer is not NUL-terminated: bound the copy.
        char body[512];
        size_t n = len < sizeof(body) - 1 ? len : sizeof(body) - 1;
        memcpy(body, data, n);
        body[n] = '\0';
        send_response(request, http_api_post_config(body));
    });
    server.on(HTTP_API_DEBUG_INFO, HTTP_GET, [](AsyncWebServerRequest *request) {
        send_response(request, http_api_get(HTTP_API_DEBUG_INFO));
    });
}

void net_broadcast(const String &status) {
    ws.textAll(status);
}

#else // no network (e2e simulation, native tests)

void net_init() {}
void net_broadcast(const String &) {}

#endif
