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

void test_get_base_mac_native() {
    // No eFuse on the host: the native twin returns the all-zero address
    // (the device body's little-endian extraction is covered by the SSID
    // test above, which pins the same byte order).
    uint8_t mac[6];
    memset(mac, 0xAA, sizeof(mac));
    getBaseMac(mac);
    const uint8_t zeros[6] = { 0 };
    TEST_ASSERT_EQUAL_HEX8_ARRAY(zeros, mac, 6);
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_format_default_ssid);
    RUN_TEST(test_format_default_ssid_length);
    RUN_TEST(test_get_base_mac_native);
    return UNITY_END();
}
