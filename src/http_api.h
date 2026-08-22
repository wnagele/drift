#ifndef DRIFT_HTTP_API_H
#define DRIFT_HTTP_API_H

#include <Arduino.h>

#include <stddef.h>
#include <stdint.h>

// Host-testable seam for the device HTTP API served by net.cpp. net.cpp
// owns the transports (WiFi, HTTP server, WebSocket, restart); this module
// owns the routes - paths, statuses, content types and payloads - so the
// API contract can be asserted natively against the shared fixtures
// (test/fixtures/api/*.json) instead of only existing implicitly in the
// server callbacks and mirrored by the dash tests.

struct HttpApiResponse {
    bool handled;              // false: no route (the server 404s)
    int status;                // HTTP status code
    const char *content_type;  // NULL: none
    const uint8_t *body;       // binary body (the dash blob); NULL: none
    size_t body_len;
    String body_text;          // text/JSON body; empty: none
    bool gzip;                 // response carries Content-Encoding: gzip
    bool restart_after;        // restart the device once the response is sent
};

// Route paths: the single source for both the seam (below) and the server
// registration in net.cpp, so the routes under test are exactly the routes
// served.
#define HTTP_API_ROOT "/"
#define HTTP_API_CONFIG "/api/config"
#define HTTP_API_DEBUG_INFO "/debug/info"

// Inject the dash blob to serve on GET / (net.cpp owns the generated
// dash.cpp array, which the native tests do not link).
void http_api_init(const uint8_t *dash, size_t dash_len);

// Route a GET by path. Unknown paths return handled = false.
HttpApiResponse http_api_get(const char *path);

// POST /api/config. `body` is the NUL-terminated JSON document (NULL for a
// bodiless POST, which the API rejects with 400).
HttpApiResponse http_api_post_config(const char *body);

#endif
