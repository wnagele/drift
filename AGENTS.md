# AGENTS.md

## Project overview

DRIFT is an ESP32-C3 firmware (PlatformIO, Arduino framework) that reads
MAVLink v1 telemetry from a flight controller and broadcasts Drone Remote ID
(DRI / ASTM F3411) over BLE 4 legacy advertising, BLE 5 Long Range, a
Wi-Fi Beacon vendor IE and Wi-Fi NAN. A React dashboard is served gzipped
from the device's Wi-Fi SoftAP for config and status.

- `src/` — firmware (~1k lines of hand-written C++ across 17 modules).
  Hardware/platform code is confined to `#ifdef` blocks at the bottom of a file
  or to a whole-file no-op twin, so every module also compiles on the host
  (`native` test env). Pure decisions are extracted into testable seams.
- `lib/libopendroneid/` — vendored opendroneid-core-c (`opendroneid.{c,h}` plus
  `wifi.c`/`odid_wifi.h`, the Wi-Fi Beacon and Wi-Fi NAN frame builders), the
  reference
  ODID encoder. Picked up automatically as a PlatformIO lib. Two small local
  patches (marked `DRIFT local patch` in `wifi.c`) must be re-applied on
  refresh: a macOS `byteswap.h` fallback and the `<time.h>` include for
  Arduino core 3.x.
- `dash/` — React 18 + antd dashboard, bundled and gzipped into `src/dash.{h,cpp}`.
- `test/` — host unit tests, fixtures + generators, and the QEMU/gdb e2e suite.
- `site/` — Jekyll documentation site (deployed separately, excluded from the
  firmware CI).

## Firmware architecture

Data flow, one byte of UART per `loop()` iteration:

```
UART (Serial0 on device, Serial1 in e2e) 9600 baud
  -> betaflight_mavlink.cpp  mavlink_parse_byte()      -> mavlink_state_t
  -> main.cpp loop()         MAVLink -> ODID semantics (unit conversion, range
                             clamping to ODID "unknown" sentinels, arm/fix gates)
  -> dri.cpp                 dri_update_status/location/operator() into the one
                             global ODID_UAS_Data, marking messages *Valid
  -> dri.cpp dri_transmit()  BT4: 100 ms slots (dri_due; the Core Spec
                              ADV_NONCONN_IND advertising floor), 10-slot
                              round robin (2 basic id, 4 location, 2 system,
                              1 self id, 1 operator id per second — the
                              F3411 rate allocation), vendored encoders
                             BT5: 1 s guard (dri_pack_due), one Message Pack
                             (odid_message_build_pack) of every valid message
                             Wi-Fi Beacon: 200 ms guard (dri_wifi_beacon_due), the
                              same Message Pack refreshed in the SoftAP vendor IE
                              Wi-Fi NAN: 512 ms guard (dri_wifi_nan_due), the NAN sync
                              beacon plus a Message Pack action frame built by the
                              vendored NAN builders, raw-injected on the SoftAP
  -> ble_frame.cpp           [0]=0x0D app code, [1]=msg counter, payload; the
                              raw AD structures ([len][0x16][FA FF][...]) for
                              both transports
  -> ble.cpp ble_send()      BT4: legacy PDU, service data under UUID 0xFFFA,
                              20 ms adv interval
     ble.cpp ble_send_pack() BT5 Long Range: Coded PHY S=8, ~1 Hz
     wifi_frame.cpp          the vendor IE ([0xDD][len][FA 0B BC][0x0D][counter]
                              [pack]) pinned byte-exact against the vendored
                              beacon frame builder
     wifi_beacon.cpp         esp_wifi_set_vendor_ie() on the SoftAP's beacons
                              AND probe responses (clear-then-set per refresh),
                              ~5 Hz
     wifi_nan.cpp            esp_wifi_80211_tx() of the vendored sync beacon and
                              action frames on the SoftAP interface (channel 6 =
                              the NAN cluster channel), one pair per ~512 ms
                              discovery window
```

A parallel path feeds the dashboard: `status.cpp` counters latched into booleans
by a 3 s TaskScheduler task, pushed as JSON over `/ws` by a 1 s task. The same
message carries the per-transport transmit rates from `txcount.cpp` (Remote-ID-carrying
frames the broadcast schedule handed to the radio seams over the last completed second,
and the ODID messages they carried — a NAN window's sync beacon holds no ODID data and is
not counted, so frames:messages is 1:N on every transport; counted in `dri.cpp` at the
send seams, enable-gated by the config flags wired through `txcount_init()`, sampled by
`txcount_sample(millis())` from the main loop). The same 1 s task then pushes a
second message, `{"type":"broadcast"}` from `broadcast.cpp` — the broadcast
inspector payload, i.e. every field of the one `ODID_UAS_Data` the transports
are composed from, with each enum already a display-ready string and each
measurement either a number or `null` where DRIFT is broadcasting ODID's
in-band "no value", so the dash holds no ordinal table and no sentinel table.
A message whose `*Valid` flag is clear contributes no keys at all, which gives
the dash two distinct negative states: key absent = not broadcast, key present
and `null` = broadcast as "no value". The strings are therefore API, pinned by
`test/fixtures/api/broadcast.json`. The dash derives its own
health from that cadence: App owns one `/ws` connection
(`useStatusSocket`) and passes it to two consumers. The **sidebar** carries
the whole health block (`SidebarHealth`): a connection box (green Connected /
orange Connecting or No data / red Disconnected, with the last update age)
plus the telemetry and GNSS flags (green/red, orange until the first status
message — never red, which would claim a fault the device has not reported).
All three are the same `StatusBox`, they are in the sider so they stay visible
from every tab, and on narrow screens (antd's `md` breakpoint) the block
collapses to one identity icon plus a state dot each, with the text in a
native tooltip — the icon is what keeps the collapsed block legible, since
three bare dots carry colour but no identity. An open-but-silent socket (no status message for 5 s) is
flagged "No data" so a wedged link shows up instead of freezing the flags.
Only a `status` message refreshes that liveness — a `broadcast` message
deliberately does not, so a healthy inspector stream cannot mask a wedged
status path.
The **Status view** is what the device puts on air: the per-transport
transmit-rate table, then the broadcast-content table (`Status.js`'s own
descriptor list, so the view never depends on JSON key order). That table is
a list of what goes out and nothing else — it names no ODID message (which
message carries a field is not the operator's concern), a field the payload
omits gets no row at all, and an enum that qualifies something is rendered
after the value it qualifies (`(Type: Serial Number)`, `(Type: Above
Take-off)`) rather than taking a row of its own. Three payload fields that
are read as a unit share one row each with their parts named inside the
value — the aircraft's `Location`, the `Operator location` and the
`Accuracy` trio — and the altitudes carry no datum in their label, because
F3411 declares them WGS-84 ellipsoidal while the flight controller supplies
MSL. Only an explicit `null` gets a row, reading "unknown". There are
two
menu entries only, Status and Config; the old three-view split (flags on
Status, rates on Statistics) is gone.

Module map: `main.cpp` (wiring, the only MAVLink→ODID mapping),
`betaflight_mavlink` (ingest), `dri` (ODID + all four broadcast schedules),
`ble_frame`/`ble` (on-air frames + radio), `wifi_frame`/`wifi_beacon`
(vendor IE bytes + SoftAP registration), `wifi_nan` (Wi-Fi NAN transport: raw
802.11 injection - the frames themselves are the vendored builders, no local
frame seam), `config`/`config_storage`/
`config_storage_esp` (JSON ⇄ NVS), `http_api` (transport-free routing table),
`net` (Wi-Fi, async HTTP, WebSocket, OTA), `wifi_ap` (open-vs-WPA decision),
`status`, `txcount` (per-transport transmit-rate counters for the dash),
`broadcast` (the ODID state as dash-ready JSON), `debug` (build provenance),
`utils` (default SSID + Wi-Fi NAN source MAC from eFuse MAC).

Testability seams — keep these intact when refactoring:

- `struct ConfigStorage` (`src/config_storage.h`) — three function pointers;
  production backend is `config_storage_esp()` (Preferences, namespace `drift`),
  tests inject an in-memory map. Booleans ride the string seam as `"1"`/`"0"`.
- Time is a parameter, never `millis()` inside a module: `dri_init(data, now)`,
  `dri_transmit(data, now)`, `dri_due(last, now)`, `dri_pack_due(last, now)`,
  `dri_wifi_beacon_due(last, now)`, `dri_wifi_nan_due(last, now)`.
- `http_api_init(dash, dash_len)` injects the dash blob, because the native
  build does not link the generated `dash.cpp`.
- `broadcast_get(const ODID_UAS_Data *)` takes the UAS data as a parameter
  because `odid_state` lives in `main.cpp`, which the native env excludes.
- Value structs instead of I/O: `HttpApiResponse`, `WifiApParams`, `BleAdvFrame`.
- The Wi-Fi NAN source MAC is injected: `wifi_nan_init(enabled, mac)` (main.cpp
  passes the eFuse base MAC from `utils::getBaseMac`, readable before the
  WiFi stack starts), and `wifi_nan_mac()` is what dri.cpp's calls to the
  vendored frame builders embed.
- `src/ble.cpp` has three bodies selected by preprocessor: real radio (NimBLE),
  an empty `DRIFT_NO_BLE` stub, and a native recorder (`ble_send_count`,
  `ble_send_counters[]`, `ble_send_messages[]`, `ble_pack_send_count`,
  `ble_pack_send_counters[]`, `ble_pack_send_lens[]`,
  `ble_pack_send_bytes[][]`, `ble_send_reset()` clears both records).
- `src/wifi_beacon.cpp` has the same three-body split (gated on
  `DRIFT_NO_NET` instead of `DRIFT_NO_BLE`): `esp_wifi_set_vendor_ie` on the
  device, an empty e2e stub, and a native recorder (`wifi_beacon_send_count`,
  `wifi_beacon_send_counters[]`, `wifi_beacon_send_lens[]`,
  `wifi_beacon_send_bytes[][]`, `wifi_beacon_send_reset()`).
- `src/wifi_nan.cpp` has the same three-body split (also `DRIFT_NO_NET`):
  `esp_wifi_80211_tx` on the device, empty e2e stubs, and native recorders
  (`wifi_nan_sync_send_*`, `wifi_nan_action_send_*`,
  `wifi_nan_send_reset()` clears both records). The frames are built by the
  vendored NAN builders in dri.cpp, so unlike the Wi-Fi Beacon IE there is no
  local frame-builder seam - the native tests pin the recorder bytes directly
  against the vendored builders' output.

## Build environments and flags

| Env | Purpose |
| --- | --- |
| `esp32_c3_devkit` | Device build. `DRIFT_MAVLINK_UART0`, USB CDC console. |
| `esp32_c3_e2e` | QEMU build. `DRIFT_NO_NET`, `DRIFT_NO_BLE`, `DRIFT_MAVLINK_UART1`, `flash_mode = dio`. |
| `native` | Host unit tests. Excludes `main.cpp`, `dash.cpp`, `config_storage_esp.cpp`; defines `ARDUINO=10805` so ArduinoJson works with ArduinoFake's `String`. |

`DRIFT_MAVLINK_UART0`/`UART1` pick the telemetry serial port; defining neither is
a hard `#error`. `DEBUG_VERSION`/`DEBUG_GIT_REF`/`DEBUG_BUILD_TIME` are not
build flags — CI `sed`-prepends them into `src/debug.h`, and they default to JSON
`null`.

The BLE stack is `h2zero/NimBLE-Arduino` (`[env]` lib_deps), not the core's BLE
library: the core 3.x bundle ships a NimBLE host with extended advertising
compiled out and no Bluedroid, so BT5 Long Range would not link. The
`CONFIG_BT_NIMBLE_EXT_ADV` / `MYNEWT_VAL_BLE_MULTI_ADV_INSTANCES` build flags
switch the ext-advertising code and the two advertising instances on. Both
transports run as extended-advertising instances (BT4 as a legacy PDU): the BT
spec forbids mixing the legacy and extended advertising enable commands, and
instance data is updated while running via the raw `ble_gap_ext_adv_set_data`
(`setInstanceData` re-configures, which the host rejects with `BLE_HS_EBUSY`
while advertising).

## Build & test commands

- Firmware build: `pio run -e esp32_c3_devkit`
- Firmware unit tests (host): `pio test -e native`
- Firmware e2e (QEMU in Docker): `make e2e`
  (or the pieces: `make e2e-firmware`, `make e2e-image`)
- Regenerate fixtures: `make fixtures` (run from the repo root)
- Dash unit tests: `cd dash && npm ci && npm test`
- Dash E2E: `cd dash && npm run test:e2e` (needs `npx playwright install chromium`)
- Dash bundle: `cd dash && ./build.sh` (npm install + webpack + wrap)
- Dash local preview: `cd dash && npm run dev` — mock device on
  `http://127.0.0.1:8321` (shared fixtures for the API, 1 Hz `/ws` stream of
  both messages, status then broadcast, like the firmware's one task).
  `WS_MODE` picks the connection-box scenario: `connected`
  (default), `no-data` (socket opens, nothing arrives), `disconnected`
  (dropped right away), `flaky` (random delays that sometimes breach the
  5 s staleness threshold, flipping the box between Connected and No data)

## Generated files (do not hand-edit)

Marked `linguist-generated` in `.gitattributes`; regenerate and commit the output.

- `src/dash.{h,cpp}` — the gzipped `index.html` + `bundle.js` as a `PROGMEM`
  byte array. `dash/build.sh` does *not* copy them; the full step is
  `cd dash && npm run webpack && node wrap.js && cp dash.{h,cpp} ../src/`.
  `wrap.js` must run with cwd `dash/`. Note the `%%%SCRIPT%%%` substitution uses
  a function replacer on purpose — a minified bundle contains `$&`/`$1`.
- `test/fixtures/mavlink/*.bin` (9) and `test/fixtures/odid/*.bin` (6) — from
  `test/tools/gen_*.cpp` via `make fixtures`. The generators write hardcoded
  relative paths, so they only work from the repo root. MAVLink headers come
  from `.pio/libdeps/native/MAVLink` (the Makefile bootstraps them with
  `pio pkg install -e native`).

`test/fixtures/api/*.json` is **hand-maintained**, not generated.

## Testing

### Host unit tests (`pio test -e native`)

Unity with a hand-written `main()` per file; ArduinoFake where Arduino APIs are
touched. Each `test/test_*/` dir is a separate target: `broadcast`, `config`,
`debug`, `dri` (largest — byte-exact ODID encodings, schedule, timing guard,
`millis()` wraparound), `http_api`, `mavlink`, `status`, `txcount`, `utils`,
`wifi_ap`.
`test/support/fixtures.h` provides `fixture_read()`, which tries several
candidate paths so the tests tolerate the runner's working directory.

### Firmware e2e (`make e2e`)

The `esp32_c3_e2e` firmware runs in Espressif's QEMU fork (`machine esp32c3`)
inside a container; `test/e2e/firmware.test.py` drives it from `gdb -batch`.
Breakpoints are the synchronisation mechanism instead of serial timeouts, and
assertions read target memory directly, so state that never reaches the serial
port (decoded ODID location, `armed`/`gps_fix`, message counters, BLE payloads)
is checkable. The only input wire is MAVLink fixtures replayed into UART1 over a
TCP chardev; UART0 is captured to a log file.

- Scenarios live in `test/e2e/scenarios/*.py`, register via `@scenario(...)`, and
  run in the canonical order listed in `firmware.test.py`. That file refuses to
  run if a scenario file on disk is missing from the list. Narrative scenarios
  deliberately share boot state, so order matters.
- `entrypoint.sh` boots three times: seeded NVS (full narrative), blank NVS
  (`defaults`), then the *same image again* to prove the defaults persisted.
- `scenarios/serial.py` runs in every boot and fails the suite on any `[E]`,
  `E (nnn)` or `assert failed` line in the UART0 log.
- Env knobs (all defaulted): `DRIFT_WORKSPACE`, `DRIFT_SCENARIOS`,
  `DRIFT_RUN_LABEL`, `DRIFT_GDB_TARGET`/`DRIFT_GDB_PORT`,
  `DRIFT_MAVLINK_HOST`/`PORT`, `DRIFT_SERIAL_LOG`, `TIMEOUT`, `FLASH_SIZE`.

### Dash tests

Vitest + jsdom + Testing Library, msw for HTTP, hand-rolled `MockWebSocket`
classes for `/ws`. `vitest.config.js` forces esbuild's `jsx` loader for `src/**`
because JSX lives in `.js` files. Playwright (`chromium` only, serial) serves the
built bundle from `e2e/page-server.js` on `127.0.0.1:8321`; the tests stub all
API and WebSocket traffic with `page.route`/`page.routeWebSocket`, which
intercept before the network. The server itself also answers those endpoints
with the shared fixtures — `npm run dev` serves a full mock device for local
visual checks (`WS_MODE` picks the connection-box scenario: `connected`,
`no-data`, `disconnected`, `flaky`).

### The shared contract

`test/fixtures/api/{config,status,broadcast,debug-info}.json` is the single
source of truth for the HTTP/WS payload shapes, asserted by the firmware unit tests, the e2e NVS
seed, and the dash unit + E2E tests. `status.json` is asymmetric
(`telemetry: true, gnss: false`) on purpose so a telemetry/gnss swap cannot
cancel out. ODID encodings come from the vendored C library and MAVLink captures
from the vendored MAVLink headers; there is intentionally no Python generator for
ODID (no maintained implementation exists).

### API surface

| Route | Response |
| --- | --- |
| `GET /` | gzipped `DASH` blob, `text/html; charset=utf-8` |
| `GET /api/config` | `{wifi:{ssid,password},dri:{region,ua_id,ua_desc,op_id,op_secret,bt5_enabled,wifi_beacon_enabled,wifi_nan_enabled}}` |
| `POST /api/config` | 200 + reboot (identity is read once at boot); 400 on a bodiless POST, malformed JSON, a missing or wrong-typed field, or an unknown region |
| `GET /debug/info` | `{version,git_ref,build_time}` (nulls in dev builds) |
| `WS /ws` | Two messages once per second, from the one 1 s task. `{type:"status",telemetry,gnss,tx:{bt4,bt5,wifi_beacon,wifi_nan}}` — each `tx` transport is `{frames,messages}` per second over the last completed window — then `{type:"broadcast",...}`, the flat broadcast inspector payload (`broadcast.cpp`; enums as display strings, measurements as numbers or `null`, an invalid message's keys omitted). Pinned by `test/fixtures/api/broadcast.json`. |

`POST /api/config` takes a **complete** configuration document: every field in
the `GET` shape must be present and correctly typed (strings for the strings —
empty is fine, `null` is not — and real JSON booleans for the three transport
flags). Partial/delta applies are rejected, not merged. `config_save()`
validates the whole document before the first `putString`, so a rejected POST
writes nothing at all and does not reboot, and returns `false` for `http_api`
to turn into the 400. This replaced two warts: a missing field used to be
persisted as the literal string `"null"` (so `POST {}` wiped the identity and
the SSID), and any body `config_save()` could not use still answered 200 and
rebooted, which a client cannot tell from success.

NVS keys (namespace `drift`): `wifi_ssid` (defaults to `DRIFT_xxxx` from the
eFuse MAC, also used as the BLE device name), `wifi_password` (empty ⇒ open AP),
`dri_region`, `dri_ua_id` (≤20), `dri_ua_desc` (≤23), `dri_op_id` (≤20),
`dri_op_secret` (the EU/UK verification code; never broadcast),
`bt5_enabled` (`"1"`/`"0"`, default on), `wifi_beacon` (same encoding, default
on; the API field is `wifi_beacon_enabled`), `wifi_nan` (same encoding, default
**off** — the one opt-in transport; the API field is `wifi_nan_enabled`).
Over-length DRI values are silently ignored by `dri_populate_identity()`.

### Region is opaque to the firmware

`dri_region` is one of `"US"`/`"EU"`/`"UK"`/`""`. Region has no default,
because DRIFT cannot guess the jurisdiction and guessing wrong is a compliance
problem rather than an inconvenience, so an unconfigured device claims nothing
and reports `""`.

`""` is also a **selectable choice** — the dash offers it as *"Other — no
regional rules"*, which is the correct answer for every jurisdiction DRIFT does
not model (Australia and Canada have no mandate in force at all) and the escape
hatch if the EU/UK syntax check ever rejects a number it should not. Under it
the operator ID is opaque: no format rules, not required, capped at
`ODID_ID_SIZE`, and **not** split into number + verification code — there is no
registry in play, so splitting would silently move the tail of a plain
20-character ID into `op_secret`.

The consequence is deliberate and worth knowing: because "Other" and
never-configured are the same value, **the firmware cannot tell them apart**,
and a factory-fresh device loads showing "Other" rather than prompting for a
region. Note also that the dash's region rule is *not* antd's `required`, since
that treats `""` as empty — only a genuinely unset value (a failed GET) is an
error.

Nothing in the firmware branches on the value. US, EU and UK differ in **no**
wire element, schedule rate or transport requirement — 14 CFR §89.315 and
Annex Part 6 of Regulation (EU) 2019/945 (and its retained UK equivalent) ask
for the same data set, and none of the three names a PHY. Region only decides
which
config inputs are required and how they are validated, so all of that (required
fields, labels, the EU/UK 16-character registration-number format and its
optional Luhn mod-36 check) lives in the **dash**. Keeping it there avoids
carrying a 31-entry ISO country table and a checksum implementation in both C++
and JS, in a build already at ~82% of its app partition.

The firmware's only region rule is the three-value allow-list in `config.cpp`,
which exists so an unservable value cannot be persisted — not so anything can
act on it. `config_dri_region()` is there to serve the value back to the dash.
Consequence to keep in mind: the dash *is* a compliance-relevant component, and
`POST /api/config` via `curl` bypasses every content rule.

Concretely, exactly **one** config field is region-dependent: the operator
registration number (`op_id`). §89.315 lists no such field for a US broadcast
module, so the dash hides it and posts `""` for both halves under `US`; EU and
UK both require it, with different labels (EASA's "operator registration
number" vs the CAA's "Remote ID number") and different prefixes. Everything
else — the serial (`ua_id`, required by all three), the Self-ID (`ua_desc`,
required by none) and the three transport flags (no region mandates any
transport) — is uniform. In particular there is no US "BT4 and BT5
simultaneously" rule and no EU Wi-Fi Beacon requirement: §89.320(g) asks only
for a non-proprietary broadcast specification on Part-15-compliant ISM-band
RF, and the EU text names no transport at all.

### The EU/UK registration number lives in dash/src/registration.js

Pinned by `dash/src/__tests__/registration.test.js` against EASA's own worked
example (`FIN87astrdge12k8-xyz`, from AMC1/GM1 to Article 14(6) of
Implementing Regulation (EU) 2019/947, which is where the 16-character format
and the Luhn mod-36 scheme are defined). Three things there are deliberate and
easy to "fix" wrongly:

- **The allow-list is 31 codes, not the EU 27.** The four EFTA states
  (`ISL`, `LIE`, `NOR`, `CHE`) participate under Article 129 of Regulation
  (EU) 2018/1139 and the same drone rules apply to them. Since the syntax
  check *blocks* saving, omitting them would make DRIFT unconfigurable for a
  Norwegian or Swiss operator. (EASA membership is confirmed; that each of
  those registries issues alpha-3-prefixed numbers in this format is **not**
  independently verified.)
- **A missing verification code is neutral, never a failure.** The Luhn
  mod-36 check needs the 3 secret digits, and national registries appear
  largely not to issue them (Austria issues only the 16-character number), so
  most compliant operators have none. The check has three states —
  passed/failed/**skipped** — and skipped renders grey.
- **The single input is split, not length-capped.** EASA's full registration
  string is 20 characters, which is *exactly* `ODID_ID_SIZE` — so a pasted
  full string used to fit a 20-char field perfectly and would have broadcast
  the private key, which the CAA explicitly warns against. The dash sanitizes
  (strips whitespace and dashes, covering both EASA's hyphenated and the
  CAA's space-separated renderings), takes the leading 16 characters as
  `op_id` and the remainder as `op_secret`, and rejoins them for display.
  Only `op_id` ever reaches `OperatorID.OperatorId`.

`op_secret` is stored and served back so the dash can rejoin it and re-run the
checksum after a reload. Note what that means: it is returned in plaintext by
an unauthenticated endpoint on a SoftAP that may be open — the same exposure
`wifi.password` already has. It is never broadcast and the firmware never
reads it for anything.

## CI

`.github/workflows/build-pr.yml` — jobs `fixtures-fresh` (`make fixtures` then
`git diff --exit-code -- test/fixtures`), `firmware-test`, `firmware-e2e`,
`dash-test`, `dash-e2e`, `build` (packages full + OTA binaries with esptool).
`create-release.yml` builds a draft release on `v*` tags. `deploy-site.yml`
publishes `site/` to Pages. PRs touching only `site/**` skip the build workflow.

## Code style

- Firmware: plain C++ functions, `snake_case` with a module prefix acting as a
  namespace (`dri_*`, `config_*`, ...); `PascalCase` structs; Arduino `String`
  at text-producing boundaries and `const char *` for literals and C APIs.
  Module state is `static` at file scope; the only intentionally external
  globals are the `status` flags and the native `ble_send_*` /
  `wifi_beacon_send_*` / `wifi_nan_send_*` recorders.
- Keep `#ifdef` blocks whole-function or whole-file, never mid-function, and
  prefer extracting the pure decision over adding a guard.
- Comments explain *why* — the existing ones document real constraints
  (QEMU flash quirks, gdb reading `ble_send`'s registers, `String` vs
  `std::string` NUL survival). Preserve them; don't replace them with restatements.
- Dash: React function components as arrow consts with a bottom
  `export default`, local `useState`/antd `Form` state only (no store, no
  router), bare `axios` in `useEffect` with `try/catch/finally` and antd
  `message.*` toasts. Toast strings are asserted verbatim by the tests.
- Keep changes minimal; match the surrounding style.

## Gotchas

- **e2e flash mode must stay `dio`.** QEMU's `m25p80` mishandles the QIO
  fast-read dummy bytes, so NVS silently reads garbage and config storage stays
  empty.
- **The e2e flash image must stay padded to 16 MB.** The `esp32c3` machine picks
  the emulated flash chip from the image size; only the 16 MB (ISSI) model
  emulates the Quad-Enable bit, otherwise `esp_flash_init` aborts.
- **`ble_send()`'s and `ble_send_pack()`'s `DRIFT_NO_BLE` bodies must stay empty
  and in their own TU.** The gdb harness reads `$a0`/`$a1` (`$a2` for the pack
  length) at function entry to capture broadcasts. Same for
  `wifi_beacon_send_pack()`'s and the `wifi_nan_send_*()`s' `DRIFT_NO_NET`
  bodies — and for `net_broadcast()`'s, whose `$a0` is how
  `scenarios/status.py` reads the pushed `/ws` payload back
  (`read_arduino_string_ref`) to tell the status and broadcast messages
  apart.
- **The vendor IE refresh must stay a clear-then-set.** `esp_wifi_set_vendor_ie`
  rejects enabling an IE at an index that is already enabled
  (`ESP_ERR_INVALID_ARG`), so every ~5 Hz refresh clears the BEACON and
  PROBE_RESP IEs before re-registering them; and the `vendor_ie_data_t` payload
  must follow the 6-byte header inline in one buffer (flexible-array struct —
  the pitfall behind ArduRemoteID issue #155), which is why
  `wifi_frame_build_vendor_ie()` emits exactly those bytes.
- **NVS keys are capped at 15 characters** (Preferences, `nvs_partition_gen`),
  so the Wi-Fi Beacon enable is stored as `wifi_beacon` while its API field is
  `wifi_beacon_enabled`. Keep new keys short; the e2e seed writes the same
  strings (test/e2e/nvs_seed.py).
- **The C3 has no NAN engine** (`SOC_WIFI_NAN_SUPPORT` is absent from its
  `soc_caps.h`; `esp_wifi_nan_*` exists only on ESP32/S2/C5/C61), which is why
  the Wi-Fi NAN transport raw-injects the vendored builders' frames via
  `esp_wifi_80211_tx(WIFI_IF_AP, ..., en_sys_seq=true)` instead of using a NAN
  stack — the same approach ArduRemoteID ships. The SoftAP's channel 6 pin is
  what makes that legal: 6 is the NAN cluster channel. Wi-Fi NAN defaults to
  off (opt-in config) because every regional profile is satisfied without it.
- **The Wi-Fi NAN action-frame counter advances only on a transmitted frame.**
  `odid_wifi_build_message_pack_nan_action_frame()` embeds it twice (service
  info `message_counter` at offset 43 and the trailing `service_update_
  indicator`), and dri.cpp increments it only when the builder accepts the
  pack, so a skipped window does not burn counter values the receiver would
  read as lost frames.
- **The reference-aircraft constants are duplicated** in
  `test/tools/gen_odid_fixtures.cpp`, `test/test_dri/test_dri.cpp` and
  `test/fixtures/api/config.json`. Changing one requires changing the others.
- `test_mavlink` runs its partial-frame test first on purpose; `mavlink_reset()`
  also clears the MAVLink library's global per-channel parser state.
- ODID fixtures end in `0x00` and buffers are pre-zeroed, so truncation is
  invisible — the payload tests use non-zero fill deliberately.
- `test_config`'s rejection tests all assert *both* halves of the
  complete-document contract: that the save was refused, and that every other
  field still holds its previous value. The second half is what pins the
  validate-before-write ordering, so a rejection cannot leave storage
  half-updated. The old characterization tests for the `"null"`-string
  behaviour are gone, because the behaviour is.
- `native` needs `-Wno-missing-template-arg-list-after-template-kw` on Apple
  clang 21+ for ArduinoFake's `fakeit.hpp`.
- The `-diff` attribute on `src/dash.cpp`, `dash/package-lock.json` and the
  fixture `.bin`s is intentional; text diffs stay disabled for them.

## Before opening a PR

`pio run -e esp32_c3_devkit`, `pio test -e native`, `make e2e`, `make fixtures`
(clean diff), and the dash unit + E2E suites. Regenerate generated files instead
of editing them, and commit the outputs.
