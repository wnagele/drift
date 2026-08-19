#include <unity.h>
#include <Arduino.h>

#include "utils.h"

void test_format_default_ssid() {
    // Bytes 4 and 5 of the eFuse MAC (counting from LSB) are rendered.
    TEST_ASSERT_EQUAL_STRING("DRIFT_BBAA", formatDefaultSSID(0xAABBCCDDEEFF).c_str());
    TEST_ASSERT_EQUAL_STRING("DRIFT_0000", formatDefaultSSID(0).c_str());
    TEST_ASSERT_EQUAL_STRING("DRIFT_AABB", formatDefaultSSID(0x0000BBAA00000000ULL).c_str());
}

void test_format_default_ssid_length() {
    // DRIFT_XXXX is 10 chars; the buffer must hold 10 + NUL.
    TEST_ASSERT_EQUAL(10, strlen(formatDefaultSSID(0xAABBCCDDEEFF).c_str()));
    TEST_ASSERT_EQUAL(11, DEFAULT_SSID_LENGTH);
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_format_default_ssid);
    RUN_TEST(test_format_default_ssid_length);
    return UNITY_END();
}
