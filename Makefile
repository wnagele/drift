CXX ?= g++
PIO ?= pio
BUILD_DIR := .build
MAVLINK_DIR := .pio/libdeps/native/MAVLink

E2E_IMAGE := drift-e2e

MAVLINK_FIXTURES := test/fixtures/mavlink/disarmed_fix.bin \
                    test/fixtures/mavlink/armed_no_fix.bin \
                    test/fixtures/mavlink/armed_fix.bin \
                    test/fixtures/mavlink/armed_unknown.bin \
                    test/fixtures/mavlink/no_fix_position.bin \
                    test/fixtures/mavlink/fix_2d.bin \
                    test/fixtures/mavlink/emergency.bin \
                    test/fixtures/mavlink/undeclared.bin \
                    test/fixtures/mavlink/origin_set.bin \
                    test/fixtures/mavlink/accuracy_v2.bin
ODID_FIXTURES := test/fixtures/odid/basic_id.bin \
                  test/fixtures/odid/self_id.bin \
                  test/fixtures/odid/operator_id.bin \
                  test/fixtures/odid/system.bin \
                  test/fixtures/odid/location.bin \
                  test/fixtures/odid/location_unknown.bin

fixtures: $(MAVLINK_FIXTURES) $(ODID_FIXTURES)

$(BUILD_DIR):
	mkdir -p $@

# MAVLink headers come from the native PlatformIO env's libdeps.
$(MAVLINK_DIR):
	pio pkg install -e native

$(BUILD_DIR)/gen_mavlink_fixtures: test/tools/gen_mavlink_fixtures.cpp | $(BUILD_DIR) $(MAVLINK_DIR)
	$(CXX) -std=c++11 -Wno-address-of-packed-member -I$(MAVLINK_DIR) $< -o $@

$(BUILD_DIR)/gen_odid_fixtures: test/tools/gen_odid_fixtures.cpp lib/libopendroneid/opendroneid.c lib/libopendroneid/opendroneid.h | $(BUILD_DIR)
	$(CXX) -std=c++11 -Ilib/libopendroneid \
	    test/tools/gen_odid_fixtures.cpp lib/libopendroneid/opendroneid.c -o $@

$(MAVLINK_FIXTURES): $(BUILD_DIR)/gen_mavlink_fixtures
	mkdir -p test/fixtures/mavlink
	$(BUILD_DIR)/gen_mavlink_fixtures

$(ODID_FIXTURES): $(BUILD_DIR)/gen_odid_fixtures
	mkdir -p test/fixtures/odid
	$<

# End-to-end tests: the firmware runs in Espressif's QEMU fork (ESP32-C3) in a
# container, test/e2e/firmware.test.py drives it over gdb and a failed check
# fails the build. See test/e2e/docker.
e2e-firmware:
	$(PIO) run -e esp32_c3_e2e

# Regenerate the embedded dash (src/dash.{h,cpp}) from the dash/ sources:
# webpack then wrap.js, then the copy into src/ - which the former
# dash/build.sh never did. npm ci runs first when node_modules is missing,
# so the target works from a fresh checkout.
#
# dash-check: freshness without a byte-exact diff. wrap.js embeds a sha256
# of the bundle's *inputs* (dash/src, index.html, package-lock.json) into
# the generated files; the check recomputes it and compares. A byte-exact
# diff would false-positive across environments: webpack's minimized output
# is not byte-stable across node versions, so the committed blob only
# reproduces under the node major that last regenerated it - the inputs
# hash is the invariant that actually matters.
DASH_NODE_MODULES := dash/node_modules

$(DASH_NODE_MODULES):
	cd dash && npm ci

.PHONY: dash dash-check
dash: | $(DASH_NODE_MODULES)
	cd dash && npm run webpack
	cd dash && node wrap.js
	cp dash/dash.h dash/dash.cpp src/

dash-check: | $(DASH_NODE_MODULES)
	cd dash && node wrap.js --check

e2e-image:
	docker build -f test/e2e/docker/Dockerfile -t $(E2E_IMAGE) test/e2e/docker

e2e: e2e-firmware e2e-image
	docker run --rm -v "$(CURDIR):/work:ro" $(E2E_IMAGE)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: fixtures clean e2e-firmware e2e-image e2e
