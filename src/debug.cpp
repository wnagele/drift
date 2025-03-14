#include <ArduinoJson.h>

#include "debug.h"

#ifndef DEBUG_VERSION
#define DEBUG_VERSION serialized("null")
#endif
#ifndef DEBUG_GITREF
#define DEBUG_GITREF serialized("null")
#endif

String debug_info() {
    JsonDocument doc;
    doc["version"] = DEBUG_VERSION;
    doc["git_ref"] = DEBUG_GITREF;
    String buf;
    serializeJson(doc, buf);
    return buf;
}
