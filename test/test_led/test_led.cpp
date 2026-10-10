#include <unity.h>

#include "led.h"

// status.cpp's health flags (no getters - the booleans are the API).
extern bool telemetry;
extern bool gnss;

void setUp() {
    // Back to boot: no status, ADVANCED board, cleared recorder.
    telemetry = false;
    gnss = false;
    led_init(BOARD_TYPE_ADVANCED);
    led_write_reset();
}

void test_resolve_table() {
    // GOOD: both halves up.
    TEST_ASSERT_EQUAL(LED_STATE_GOOD, led_resolve(true, true, false, false));
    TEST_ASSERT_EQUAL(LED_STATE_GOOD, led_resolve(true, true, true, true));
    // ACQUIRING: no complete picture yet - nothing heard, or a link
    // without a fix (indoors, cold start). Not an error.
    TEST_ASSERT_EQUAL(LED_STATE_INIT, led_resolve(false, false, false, false));
    TEST_ASSERT_EQUAL(LED_STATE_INIT, led_resolve(true, false, false, false));
    TEST_ASSERT_EQUAL(LED_STATE_INIT, led_resolve(true, false, true, false));
    // ERROR: the link or the fix went stale after having been up.
    TEST_ASSERT_EQUAL(LED_STATE_ERROR, led_resolve(false, false, true, false));  // link lost
    TEST_ASSERT_EQUAL(LED_STATE_ERROR, led_resolve(true, false, true, true));    // fix lost
    TEST_ASSERT_EQUAL(LED_STATE_ERROR, led_resolve(false, true, true, true));
}

void test_pins_for() {
    TEST_ASSERT_EQUAL_INT8(LED_PIN_NONE, led_pins_for(BOARD_TYPE_BASIC).red);
    TEST_ASSERT_EQUAL_INT8(LED_PIN_NONE, led_pins_for(BOARD_TYPE_BASIC).green);
    TEST_ASSERT_EQUAL_INT8(LED_PIN_NONE, led_pins_for(BOARD_TYPE_UNDEFINED).red);
    TEST_ASSERT_EQUAL_INT8(LED_PIN_NONE, led_pins_for(BOARD_TYPE_UNDEFINED).green);
    // ADVANCED: IO4 = green, IO5 = red (led.cpp's pin table).
    TEST_ASSERT_EQUAL_INT8(5, led_pins_for(BOARD_TYPE_ADVANCED).red);
    TEST_ASSERT_EQUAL_INT8(4, led_pins_for(BOARD_TYPE_ADVANCED).green);
}

void test_toggle_due() {
    // Same guard idiom as dri_due(): strictly after a full interval.
    TEST_ASSERT_FALSE(led_toggle_due(1000, 2000));
    TEST_ASSERT_TRUE(led_toggle_due(1000, 2001));
    // millis() wraparound. Values stay under 2^32 so the subtraction wraps
    // the same way on 32- and 64-bit builds (unsigned long is 32-bit on the
    // ESP32 but 64-bit on the host - the test_dri wraparound tests' idiom).
    TEST_ASSERT_FALSE(led_toggle_due(0xFFFFF000UL, 0xFFFFF000UL + 1000UL));
    TEST_ASSERT_TRUE(led_toggle_due(0xFFFFF000UL, 0xFFFFF000UL + 1001UL));
}

void test_boot_alternates_at_half_hertz() {
    led_update(0);      // first update lights red immediately (INIT starts red)
    led_update(999);   // inside the toggle guard: no write
    led_update(1001);  // guard fires: green
    led_update(1500);  // no write
    led_update(2002);  // guard fires: red again
    TEST_ASSERT_EQUAL_UINT32(3, led_write_count);
    TEST_ASSERT_EQUAL_UINT8(LED_COLOR_RED, led_write_colors[0]);
    TEST_ASSERT_EQUAL_UINT8(LED_COLOR_GREEN, led_write_colors[1]);
    TEST_ASSERT_EQUAL_UINT8(LED_COLOR_RED, led_write_colors[2]);
}

void test_good_writes_green_once_then_error_red() {
    telemetry = true;
    gnss = true;
    led_update(0);
    led_update(500);   // GOOD is solid: no second write
    TEST_ASSERT_EQUAL_UINT32(1, led_write_count);
    TEST_ASSERT_EQUAL_UINT8(LED_COLOR_GREEN, led_write_colors[0]);
    // Link and fix both stale now: red, and never green again without both.
    telemetry = false;
    gnss = false;
    led_update(1000);
    TEST_ASSERT_EQUAL_UINT32(2, led_write_count);
    TEST_ASSERT_EQUAL_UINT8(LED_COLOR_RED, led_write_colors[1]);
    telemetry = true;
    led_update(1100);  // link back, fix still stale: red holds
    TEST_ASSERT_EQUAL_UINT32(2, led_write_count);
    gnss = true;
    led_update(1200);
    TEST_ASSERT_EQUAL_UINT32(3, led_write_count);
    TEST_ASSERT_EQUAL_UINT8(LED_COLOR_GREEN, led_write_colors[2]);
}

void test_link_without_fix_keeps_alternating() {
    telemetry = true;   // heartbeat arriving, no fix yet
    led_update(0);      // acquiring: alternation starts red
    led_update(500);
    led_update(1001);   // toggle guard fires: green
    TEST_ASSERT_EQUAL_UINT32(2, led_write_count);
    TEST_ASSERT_EQUAL_UINT8(LED_COLOR_RED, led_write_colors[0]);
    TEST_ASSERT_EQUAL_UINT8(LED_COLOR_GREEN, led_write_colors[1]);
    // The fix arrives mid-green: GOOD is the same colour, no new write.
    gnss = true;
    led_update(1500);
    led_update(2500);
    TEST_ASSERT_EQUAL_UINT32(2, led_write_count);
    // The fix goes stale again: red.
    gnss = false;
    led_update(3000);
    TEST_ASSERT_EQUAL_UINT32(3, led_write_count);
    TEST_ASSERT_EQUAL_UINT8(LED_COLOR_RED, led_write_colors[2]);
}

void test_no_leds_is_a_noop() {
    static const board_type_t ledless[] = { BOARD_TYPE_BASIC, BOARD_TYPE_UNDEFINED };
    for (board_type_t type : ledless) {
        telemetry = true;
        gnss = true;
        led_init(type);
        led_write_reset();
        led_update(0);
        led_update(5000);
        TEST_ASSERT_EQUAL_UINT32(0, led_write_count);
    }
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_resolve_table);
    RUN_TEST(test_pins_for);
    RUN_TEST(test_toggle_due);
    RUN_TEST(test_boot_alternates_at_half_hertz);
    RUN_TEST(test_good_writes_green_once_then_error_red);
    RUN_TEST(test_link_without_fix_keeps_alternating);
    RUN_TEST(test_no_leds_is_a_noop);
    return UNITY_END();
}