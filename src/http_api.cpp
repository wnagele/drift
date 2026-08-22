#include <string.h>

#include "config.h"
#include "debug.h"
#include "http_api.h"

static const uint8_t *dash_blob = NULL;
static size_t dash_blob_len = 0;

void http_api_init(const uint8_t *dash, size_t dash_len) {
    dash_blob = dash;
    dash_blob_len = dash_len;
}

HttpApiResponse http_api_get(const char *path) {
    if (strcmp(path, HTTP_API_ROOT) == 0)
        return {true, 200, "text/html", dash_blob, dash_blob_len, String(), true, false};
    if (strcmp(path, HTTP_API_CONFIG) == 0)
        return {true, 200, "application/json", NULL, 0, config_get(), false, false};
    if (strcmp(path, HTTP_API_DEBUG_INFO) == 0)
        return {true, 200, "application/json", NULL, 0, debug_info(), false, false};
    return {false, 404, NULL, NULL, 0, String(), false, false};
}

HttpApiResponse http_api_post_config(const char *body) {
    if (body == NULL)
        return {true, 400, NULL, NULL, 0, String(), false, false};
    config_save(body);
    return {true, 200, NULL, NULL, 0, String(), false, true};
}
