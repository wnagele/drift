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
                    test/fixtures/mavlink/origin_set.bin
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

e2e-image:
	docker build -f test/e2e/docker/Dockerfile -t $(E2E_IMAGE) test/e2e/docker

e2e: e2e-firmware e2e-image
	docker run --rm -v "$(CURDIR):/work:ro" $(E2E_IMAGE)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: fixtures clean e2e-firmware e2e-image e2e
