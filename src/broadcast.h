#ifndef DRIFT_BROADCAST_H
#define DRIFT_BROADCAST_H

#include <Arduino.h>

#include <opendroneid.h>

// The broadcast inspector payload for the dash: what DRIFT is putting on
// air, message by message, as a JSON document pushed over /ws alongside the
// status message. The transmit-rate diagnostics (txcount.cpp) answer "is it
// transmitting?"; this answers "what is it transmitting?", which otherwise
// needs gdb or the e2e suite - neither available to a user in the field.
//
// Two rules make up the whole contract with the dash, so that the dash holds
// no ordinal table and no sentinel table:
//
//   1. every enum is a display-ready string the dash prints verbatim;
//   2. every measurement is a number, or null when DRIFT is broadcasting
//      ODID's "no value" representation.
//
// The strings are therefore API, and changing one is a breaking change: the
// vocabulary is pinned by test/fixtures/api/broadcast.json, which the
// firmware unit tests and both dash suites assert. That single cross-checked
// fixture is the point - the alternative, ordinals plus a name table in
// JavaScript, duplicates the vendored library's enums with nothing
// comparing the two halves.
//
// A message whose *Valid flag is clear has its fields omitted entirely, so
// the dash has exactly two negative states and they mean different things:
// key absent = not broadcast (nothing configured, or the encoder rejected
// it), key present and null = broadcast, carrying ODID's "no value".
//
// Note what this is not: every value here is read from ODID_UAS_Data, i.e.
// what the firmware intends to broadcast, not bytes observed leaving the
// radio. An encoder-level fault is invisible to it; that risk sits with the
// byte-exact native tests and the e2e suite.
//
// `data` is a parameter because odid_state lives in main.cpp, which the
// native test environment excludes.
String broadcast_get(const ODID_UAS_Data *data);

#endif
