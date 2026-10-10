#include <unity.h>

#include <stddef.h>

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

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_blank_efuse_is_undefined);
    RUN_TEST(test_basic);
    RUN_TEST(test_advanced);
    RUN_TEST(test_unknown_type_is_undefined);
    RUN_TEST(test_names);
    RUN_TEST(test_native_id_is_undefined);
    return UNITY_END();
}