#ifndef DRIFT_LED_H
#define DRIFT_LED_H

#include <stdint.h>

#include "board.h"

// The ADVANCED board's two status LEDs, driven from the same health booleans
// the dash sidebar shows:
//
//   green solid                  good state to fly: telemetry link + GNSS fix
//   red solid                    error: the link or the fix had been
//                                established and went stale
//   red/green alternating 0.5 Hz acquiring: no link yet, or a link without
//                                a fix (indoors, cold start)
//
// The BASIC board has no LEDs, and neither has an UNDEFINED one (a devkit, a
// QEMU guest or a unit whose eFuse was never burned), so every entry point is
// a no-op there - resolved from the board type alone, never a build flag.
#define LED_TOGGLE_INTERVAL 1000  // ms per colour: red, green, red, ... = 0.5 Hz

// Pin numbers are resolved in firmware per board type (led_pins_for), never
// read from eFuse - see board.h.
#define LED_PIN_NONE (-1)

typedef struct {
    int8_t red;
    int8_t green;
} LedPins;

typedef enum {
    LED_STATE_INIT = 0,
    LED_STATE_GOOD,
    LED_STATE_ERROR,
} led_state_t;

typedef enum {
    LED_COLOR_OFF = 0,
    LED_COLOR_RED,
    LED_COLOR_GREEN,
} led_color_t;

// Pure decisions, natively testable.
led_state_t led_resolve(bool telemetry, bool gnss, bool ever_telemetry, bool ever_gnss);
bool led_toggle_due(unsigned long last_due, unsigned long now);
LedPins led_pins_for(board_type_t type);

// Resolve the pins for this board and reset the indicator state.
void led_init(board_type_t type);
// Apply the current health (status.cpp's latched flags) to the LEDs. Cheap:
// call from loop() with millis().
void led_update(unsigned long now);

#if !defined(ESP32)
// Native tests: every colour led_update() writes, in order.
extern unsigned led_write_count;
extern uint8_t led_write_colors[];
void led_write_reset(void);
#endif

#endif