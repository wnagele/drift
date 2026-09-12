#include <Arduino.h>

#include "config_storage.h"

void config_init(const ConfigStorage *storage, const String &default_ssid);
String config_get();

// POST /api/config takes a *complete* configuration document: every field
// below must be present and correctly typed. Partial/delta applies are
// rejected rather than merged, which is what makes an all-or-nothing apply
// possible - the document is fully validated before anything is written, so
// a rejected save leaves storage untouched. Returns false on malformed JSON,
// a missing field, a wrong-typed field or an unknown region; http_api turns
// that into a 400 and skips the reboot.
bool config_save(String data);

String config_wifi_ssid();
String config_wifi_password();
String config_dri_ua_id();
String config_dri_ua_desc();
String config_dri_op_id();

// The EU/UK registration number's separately-issued 3-character verification
// code - EASA's "secret digits", the CAA's "private key". Split off the
// operator's pasted full registration string by the dash and stored apart
// from the number itself, so the thing that goes on the wire
// (OperatorID.OperatorId, fed from config_dri_op_id()) cannot accidentally
// carry it: broadcasting it would defeat the anti-cloning purpose it exists
// for, and the CAA explicitly says never to expose it. Empty whenever the
// registry issued none, which is the common case - see AGENTS.md.
String config_dri_op_secret();

// The operating jurisdiction, one of "US"/"EU"/"UK" (empty until the user
// picks one). Deliberately *opaque to the firmware*: nothing branches on it,
// because none of the three regions differs in wire encoding, schedule or
// transport set - region only decides which config inputs are required and
// how they are validated, and that lives in the dash. This getter exists so
// the value can be served back to the dash, not so the firmware can act on
// it. See the region note in AGENTS.md.
String config_dri_region();

bool config_bt5_enabled();
bool config_wifi_beacon_enabled();
bool config_wifi_nan_enabled();
