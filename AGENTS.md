# AGENTS.md

## Project overview

DRIFT is an ESP32-C3 firmware (PlatformIO, Arduino framework) that reads
MAVLink v1 telemetry from a flight controller and broadcasts Drone Remote ID
(DRI / ASTM F3411) over BLE 4 legacy advertising. A React dashboard is served
gzipped from the device's Wi-Fi SoftAP for config and status.

- `src/` — firmware (~1k lines of hand-written C++ across 13 modules).
  Hardware/platform code is confined to `#ifdef` blocks at the bottom of a file
  or to a whole-file no-op twin, so every module also compiles on the host
  (`native` test env). Pure decisions are extracted into testable seams.
- `lib/libopendroneid/` — vendored opendroneid-core-c (`opendroneid.{c,h}` plus
  `wifi.c`/`odid_wifi.h`, the Wi-Fi Beacon/NAN frame builders), the reference
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
  -> dri.cpp dri_transmit()  BT4: 40 ms guard (dri_due), 25-slot round robin
                             (1 basic id, 1 self id, 1 operator id, 1 system,
                             21 location per second), vendored encoders
                             BT5: 1 s guard (dri_pack_due), one Message Pack
                             (odid_message_build_pack) of every valid message
  -> ble_frame.cpp           [0]=0x0D app code, [1]=msg counter, payload; the
                             raw AD structures ([len][0x16][FA FF][...]) for
                             both transports
  -> ble.cpp ble_send()      BT4: legacy PDU, service data under UUID 0xFFFA,
                             20 ms adv interval
     ble.cpp ble_send_pack() BT5 Long Range: Coded PHY S=8, ~1 Hz
```

A parallel path feeds the dashboard: `status.cpp` counters latched into booleans
by a 3 s TaskScheduler task, pushed as JSON over `/ws` by a 1 s task.

Module map: `main.cpp` (wiring, the only MAVLink→ODID mapping),
`betaflight_mavlink` (ingest), `dri` (ODID + both broadcast schedules),
`ble_frame`/`ble` (on-air frames + radio), `config`/`config_storage`/
`config_storage_esp` (JSON ⇄ NVS), `http_api` (transport-free routing table),
`net` (Wi-Fi, async HTTP, WebSocket, OTA), `wifi_ap` (open-vs-WPA decision),
`status`, `debug` (build provenance), `utils` (default SSID from eFuse MAC).

Testability seams — keep these intact when refactoring:

- `struct ConfigStorage` (`src/config_storage.h`) — three function pointers;
  production backend is `config_storage_esp()` (Preferences, namespace `drift`),
  tests inject an in-memory map. Booleans ride the string seam as `"1"`/`"0"`.
- Time is a parameter, never `millis()` inside a module: `dri_init(data, now)`,
  `dri_transmit(data, now)`, `dri_due(last, now)`, `dri_pack_due(last, now)`.
- `http_api_init(dash, dash_len)` injects the dash blob, because the native
  build does not link the generated `dash.cpp`.
- Value structs instead of I/O: `HttpApiResponse`, `WifiApParams`, `BleAdvFrame`.
- `src/ble.cpp` has three bodies selected by preprocessor: real radio (NimBLE),
  an empty `DRIFT_NO_BLE` stub, and a native recorder (`ble_send_count`,
  `ble_send_counters[]`, `ble_send_messages[]`, `ble_pack_send_count`,
  `ble_pack_send_counters[]`, `ble_pack_send_lens[]`,
  `ble_pack_send_bytes[][]`, `ble_send_reset()` clears both records).

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
built bundle from `e2e/page-server.js` on `127.0.0.1:8321`; that server only
answers `GET /`, so all API and WebSocket traffic must be stubbed with
`page.route`/`page.routeWebSocket`.

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
| `GET /api/config` | `{wifi:{ssid,password},dri:{ua_id,ua_desc,op_id,bt5_enabled}}` |
| `POST /api/config` | 200 + reboot (identity is read once at boot); 400 with no body |
| `GET /debug/info` | `{version,git_ref,build_time}` (nulls in dev builds) |
| `WS /ws` | `{type:"status",telemetry,gnss}` once per second |

NVS keys (namespace `drift`): `wifi_ssid` (defaults to `DRIFT_xxxx` from the
eFuse MAC, also used as the BLE device name), `wifi_password` (empty ⇒ open AP),
`dri_ua_id` (≤20), `dri_ua_desc` (≤23), `dri_op_id` (≤20), `bt5_enabled`
(`"1"`/`"0"`, default on; a missing/null/non-boolean POST field keeps it on).
Over-length DRI values are silently ignored by `dri_populate_identity()`.

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
  globals are the `status` flags and the native `ble_send_*` recorder.
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
  length) at function entry to capture broadcasts.
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
