#include <ArduinoJson.h>

#include "debug.h"

#ifndef DEBUG_VERSION
#define DEBUG_VERSION serialized("null")
#endif
#ifndef DEBUG_GIT_REF
#define DEBUG_GIT_REF serialized("null")
#endif
#ifndef DEBUG_BUILD_TIME
#define DEBUG_BUILD_TIME serialized("null")
#endif

String debug_info() {
    JsonDocument doc;
    doc["version"] = DEBUG_VERSION;
    doc["git_ref"] = DEBUG_GIT_REF;
    doc["build_time"] = DEBUG_BUILD_TIME;
    String buf;
    serializeJson(doc, buf);
    return buf;
}
