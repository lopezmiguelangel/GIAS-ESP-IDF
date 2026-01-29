#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Color definitions
typedef enum {
    LED_OFF = 0,
    LED_RED,
    LED_GREEN,
    LED_BLUE,
    LED_WHITE
} led_color_t;

// LED control functions
void led_init(void);
void led_set_color(led_color_t color);
void led_test_sequence(void);

#ifdef __cplusplus
}
#endif

#endif // LED_CONTROL_H