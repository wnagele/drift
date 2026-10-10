#include <Arduino.h>

#include "led.h"

// status.cpp's health flags: telemetry = a heartbeat arrived in the last
// status window, gnss = a GNSS message did. The latches behind the dash
// sidebar's telemetry/gnss boxes are the LED's whole input - a module is
// only ever "good" while both are up.
extern bool telemetry;
extern bool gnss;

static LedPins pins = { LED_PIN_NONE, LED_PIN_NONE };
static bool ever_telemetry = false;
static bool ever_gnss = false;
static unsigned long last_toggle = 0;
static bool toggle_green = false;
static uint8_t last_color = LED_COLOR_OFF;

// Green only while there is a complete picture to fly with; red once a link
// or a fix that was up goes stale; alternating while no complete picture has
// existed yet - no link at all, or a link without a fix (indoors, cold
// start), is still acquiring, not an error.
led_state_t led_resolve(bool telemetry, bool gnss, bool ever_telemetry, bool ever_gnss) {
    if (telemetry && gnss)
        return LED_STATE_GOOD;
    if ((ever_telemetry && !telemetry) || (ever_gnss && !gnss))
        return LED_STATE_ERROR;
    return LED_STATE_INIT;
}

bool led_toggle_due(unsigned long last_due, unsigned long now) {
    return now - last_due > LED_TOGGLE_INTERVAL;
}

// The ADVANCED board's status LED GPIOs: IO4 = green, IO5 = red. Resolved
// here so a board revision is a code change, never an eFuse re-burn.
LedPins led_pins_for(board_type_t type) {
    switch (type) {
        case BOARD_TYPE_ADVANCED:
            return (LedPins){ 5, 4 };
        default:
            return (LedPins){ LED_PIN_NONE, LED_PIN_NONE };  // BASIC, UNDEFINED
    }
}

void led_init(board_type_t type) {
    pins = led_pins_for(type);
    ever_telemetry = false;
    ever_gnss = false;
    last_toggle = 0;
    toggle_green = false;
    last_color = LED_COLOR_OFF;
}

#if defined(ESP32)

static void led_write(uint8_t color) {
    digitalWrite(pins.red, color == LED_COLOR_RED ? HIGH : LOW);
    digitalWrite(pins.green, color == LED_COLOR_GREEN ? HIGH : LOW);
}

#else // native tests: record every colour led_update() writes

unsigned led_write_count = 0;
uint8_t led_write_colors[32];

void led_write_reset(void) {
    led_write_count = 0;
}

static void led_write(uint8_t color) {
    if (led_write_count < sizeof(led_write_colors))
        led_write_colors[led_write_count] = color;
    led_write_count++;
}

#endif

void led_update(unsigned long now) {
    if (pins.red == LED_PIN_NONE && pins.green == LED_PIN_NONE)
        return;  // no LEDs on this board: nothing to indicate
    ever_telemetry = ever_telemetry || telemetry;
    ever_gnss = ever_gnss || gnss;
    uint8_t color;
    switch (led_resolve(telemetry, gnss, ever_telemetry, ever_gnss)) {
        case LED_STATE_GOOD:
            color = LED_COLOR_GREEN;
            break;
        case LED_STATE_ERROR:
            color = LED_COLOR_RED;
            break;
        default:
            // INIT: alternate the two colours on the toggle guard
            if (led_toggle_due(last_toggle, now)) {
                last_toggle = now;
                toggle_green = !toggle_green;
            }
            color = toggle_green ? LED_COLOR_GREEN : LED_COLOR_RED;
            break;
    }
    if (color != last_color) {
        led_write(color);
        last_color = color;
    }
}