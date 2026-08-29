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
by a 3 s TaskScheduler task, pushed as JSON over `/ws` by a 1 s task. The dash
derives its own health from that cadence: App owns one `/ws` connection
(`useStatusSocket`), the sidebar carries a connection box (green Connected /
orange Connecting or No data / red Disconnected, with the last update age;
collapses to a dot on narrow screens), and the Status view carries the
telemetry/GNSS flags — an open-but-silent socket (no status message for 5 s) is
flagged "No data" so a wedged link shows up instead of freezing the flags.

Module map: `main.cpp` (wiring, the only MAVLink→ODID mapping),
`betaflight_mavlink` (ingest), `dri` (ODID + all four broadcast schedules),
`ble_frame`/`ble` (on-air frames + radio), `wifi_frame`/`wifi_beacon`
(vendor IE bytes + SoftAP registration), `wifi_nan` (Wi-Fi NAN transport: raw
802.11 injection - the frames themselves are the vendored builders, no local
frame seam), `config`/`config_storage`/
`config_storage_esp` (JSON ⇄ NVS), `http_api` (transport-free routing table),
`net` (Wi-Fi, async HTTP, WebSocket, OTA), `wifi_ap` (open-vs-WPA decision),
`status`, `debug` (build provenance), `utils` (default SSID + Wi-Fi NAN source
MAC from eFuse MAC).

Testability seams — keep these intact when refactoring:

- `struct ConfigStorage` (`src/config_storage.h`) — three function pointers;
  production backend is `config_storage_esp()` (Preferences, namespace `drift`),
  tests inject an in-memory map. Booleans ride the string seam as `"1"`/`"0"`.
- Time is a parameter, never `millis()` inside a module: `dri_init(data, now)`,
  `dri_transmit(data, now)`, `dri_due(last, now)`, `dri_pack_due(last, now)`,
  `dri_wifi_beacon_due(last, now)`, `dri_wifi_nan_due(last, now)`.
- `http_api_init(dash, dash_len)` injects the dash blob, because the native
  build does not link the generated `dash.cpp`.
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
  `http://127.0.0.1:8321` (shared fixtures for the API, 1 Hz `/ws` status
  stream). `WS_MODE` picks the connection-box scenario: `connected`
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
touched. Each `test/test_*/` dir is a separate target: `config`, `debug`, `dri`
(largest — byte-exact ODID encodings, schedule, timing guard, `millis()`
wraparound), `http_api`, `mavlink`, `status`, `utils`, `wifi_ap`.
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

`test/fixtures/api/{config,status,debug-info}.json` is the single source of truth
for the HTTP/WS payload shapes, asserted by the firmware unit tests, the e2e NVS
seed, and the dash unit + E2E tests. `status.json` is asymmetric
(`telemetry: true, gnss: false`) on purpose so a telemetry/gnss swap cannot
cancel out. ODID encodings come from the vendored C library and MAVLink captures
from the vendored MAVLink headers; there is intentionally no Python generator for
ODID (no maintained implementation exists).

### API surface

| Route | Response |
| --- | --- |
| `GET /` | gzipped `DASH` blob, `text/html` |
| `GET /api/config` | `{wifi:{ssid,password},dri:{ua_id,ua_desc,op_id,bt5_enabled,wifi_beacon_enabled,wifi_nan_enabled}}` |
| `POST /api/config` | 200 + reboot (identity is read once at boot); 400 with no body |
| `GET /debug/info` | `{version,git_ref,build_time}` (nulls in dev builds) |
| `WS /ws` | `{type:"status",telemetry,gnss}` once per second |

NVS keys (namespace `drift`): `wifi_ssid` (defaults to `DRIFT_xxxx` from the
eFuse MAC, also used as the BLE device name), `wifi_password` (empty ⇒ open AP),
`dri_ua_id` (≤20), `dri_ua_desc` (≤23), `dri_op_id` (≤20), `bt5_enabled`
(`"1"`/`"0"`, default on; a missing/null/non-boolean POST field keeps it on),
`wifi_beacon` (same encoding, default on, same missing-field rule; the API
field is `wifi_beacon_enabled`), `wifi_nan` (same encoding, default **off**
— the one opt-in transport, so the missing-field rule inverts: absent/null/
non-boolean keeps it off; the API field is `wifi_nan_enabled`). Over-length
DRI values are silently ignored by `dri_populate_identity()`.

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
  bodies.
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
- `test_config` contains characterization tests: a missing or null JSON field is
  currently persisted as the literal string `"null"`.
- `native` needs `-Wno-missing-template-arg-list-after-template-kw` on Apple
  clang 21+ for ArduinoFake's `fakeit.hpp`.
- The `-diff` attribute on `src/dash.cpp`, `dash/package-lock.json` and the
  fixture `.bin`s is intentional; text diffs stay disabled for them.

## Before opening a PR

`pio run -e esp32_c3_devkit`, `pio test -e native`, `make e2e`, `make fixtures`
(clean diff), and the dash unit + E2E suites. Regenerate generated files instead
of editing them, and commit the outputs.
