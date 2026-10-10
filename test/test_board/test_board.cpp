#include <unity.h>

#include <stddef.h>
#include <string.h>

#include "board.h"

void setUp() {
    // Native build: no eFuse, so board_init() must land on UNDEFINED - the
    // same path a board lacking eFuse details takes on the device.
    board_init();
}

void test_blank_efuse_is_undefined() {
    uint8_t raw[BOARD_EFUSE_BYTES] = { 0, 0, 0 };
    BoardId id = board_decode(raw);
    TEST_ASSERT_EQUAL(BOARD_TYPE_UNDEFINED, id.type);
    TEST_ASSERT_EQUAL_UINT8(0, id.rev_major);
    TEST_ASSERT_EQUAL_UINT8(0, id.rev_minor);
}

void test_basic() {
    uint8_t raw[BOARD_EFUSE_BYTES] = { BOARD_TYPE_BASIC, 2, 1 };
    BoardId id = board_decode(raw);
    TEST_ASSERT_EQUAL(BOARD_TYPE_BASIC, id.type);
    TEST_ASSERT_EQUAL_UINT8(2, id.rev_major);
    TEST_ASSERT_EQUAL_UINT8(1, id.rev_minor);
}

void test_advanced() {
    uint8_t raw[BOARD_EFUSE_BYTES] = { BOARD_TYPE_ADVANCED, 0, 4 };
    BoardId id = board_decode(raw);
    TEST_ASSERT_EQUAL(BOARD_TYPE_ADVANCED, id.type);
    TEST_ASSERT_EQUAL_UINT8(0, id.rev_major);
    TEST_ASSERT_EQUAL_UINT8(4, id.rev_minor);
}

void test_unknown_type_is_undefined() {
    // Types beyond the allow-list decode as UNDEFINED (revs still factual).
    static const uint8_t unknown[] = { 3, 1, 2, 0xFF, 1, 2 };
    for (size_t i = 0; i < sizeof(unknown); i += BOARD_EFUSE_BYTES) {
        BoardId id = board_decode(&unknown[i]);
        TEST_ASSERT_EQUAL(BOARD_TYPE_UNDEFINED, id.type);
        TEST_ASSERT_EQUAL_UINT8(1, id.rev_major);
        TEST_ASSERT_EQUAL_UINT8(2, id.rev_minor);
    }
}

void test_names() {
    TEST_ASSERT_EQUAL_STRING("UNDEFINED", board_type_name(BOARD_TYPE_UNDEFINED));
    TEST_ASSERT_EQUAL_STRING("BASIC", board_type_name(BOARD_TYPE_BASIC));
    TEST_ASSERT_EQUAL_STRING("ADVANCED", board_type_name(BOARD_TYPE_ADVANCED));
    // Names are support-facing (boot log) - pin them against re-spelling.
    TEST_ASSERT_EQUAL_STRING("UNDEFINED", board_type_name((board_type_t)0xFF));
}

void test_native_id_is_undefined() {
    BoardId id = board_id();
    TEST_ASSERT_EQUAL(BOARD_TYPE_UNDEFINED, id.type);
    TEST_ASSERT_EQUAL(BOARD_TYPE_UNDEFINED, board_type());
}

void test_serial_decode() {
    // Unburned region (all zero): no serial, out "".
    uint8_t raw[BOARD_SERIAL_EFUSE_BYTES] = { 0 };
    char out[BOARD_SERIAL_BYTES + 1];
    TEST_ASSERT_FALSE(board_decode_serial(raw, out));
    TEST_ASSERT_EQUAL_STRING("", out);

    // Burned: bytes 3..22, NUL-padded, NUL-terminated in out.
    memset(raw, 0, sizeof(raw));
    memcpy(raw + BOARD_SERIAL_OFFSET, "ABC-123", 7);
    TEST_ASSERT_TRUE(board_decode_serial(raw, out));
    TEST_ASSERT_EQUAL_STRING("ABC-123", out);

    // A full-width 20-char serial fits without the NUL padding.
    memset(raw, 0, sizeof(raw));
    memcpy(raw + BOARD_SERIAL_OFFSET, "01234567890123456789", 20);
    TEST_ASSERT_TRUE(board_decode_serial(raw, out));
    TEST_ASSERT_EQUAL_STRING("01234567890123456789", out);
}

void test_native_serial_is_absent() {
    // Native stub: no eFuse, so no serial, and the accessor pair agrees.
    TEST_ASSERT_EQUAL_STRING("", board_serial());
    TEST_ASSERT_FALSE(board_serial_present());
    // The test injection seam the config lock tests use.
    board_set_serial_for_test("SERIAL-001");
    TEST_ASSERT_EQUAL_STRING("SERIAL-001", board_serial());
    TEST_ASSERT_TRUE(board_serial_present());
    board_set_serial_for_test("");
    TEST_ASSERT_FALSE(board_serial_present());
    // board_init() clears whatever a previous test injected.
    board_set_serial_for_test("SERIAL-001");
    board_init();
    TEST_ASSERT_EQUAL_STRING("", board_serial());
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_blank_efuse_is_undefined);
    RUN_TEST(test_basic);
    RUN_TEST(test_advanced);
    RUN_TEST(test_unknown_type_is_undefined);
    RUN_TEST(test_names);
    RUN_TEST(test_native_id_is_undefined);
    RUN_TEST(test_serial_decode);
    RUN_TEST(test_native_serial_is_absent);
    return UNITY_END();
}