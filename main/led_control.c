#include "led_control.h"
#include "led_strip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LED_GPIO 48         /**< GPIO number where the WS2812 LED is connected */
#define LED_NUM_PIXELS 1    /**< Number of WS2812 LEDs */

static led_strip_handle_t led_strip = NULL;

/**
 * @brief Initialize the WS2812 LED.
 *
 * Configures RMT driver, allocates resources, and clears the LED strip.
 */
void led_init(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_GPIO,
        .max_leds = LED_NUM_PIXELS,
        .led_model = LED_MODEL_WS2812,
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000, /**< 10 MHz resolution */
        .flags.with_dma = false,
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    led_strip_clear(led_strip);
}

/**
 * @brief Set the color of the LED.
 *
 * @param color Predefined LED color (LED_RED, LED_GREEN, LED_BLUE, LED_WHITE, LED_OFF)
 */
void led_set_color(led_color_t color)
{
    uint8_t r = 0, g = 0, b = 0;

    switch(color) {
        case LED_RED:    r=255; g=0;   b=0;   break;
        case LED_GREEN:  r=0;   g=255; b=0;   break;
        case LED_BLUE:   r=0;   g=0;   b=255; break;
        case LED_WHITE:  r=255; g=255; b=255; break;
        case LED_OFF:
        default:         r=0;   g=0;   b=0;   break;
    }

    if (led_strip) {
        led_strip_set_pixel(led_strip, 0, r, g, b);
        led_strip_refresh(led_strip);
    }
}

/**
 * @brief Run a simple LED test sequence.
 *
 * Cycles through Red, Green, Blue, White, and Off states,
 * each for 1 second.
 */
void led_test_sequence(void)
{
    led_set_color(LED_RED);
    vTaskDelay(pdMS_TO_TICKS(1000));

    led_set_color(LED_GREEN);
    vTaskDelay(pdMS_TO_TICKS(1000));

    led_set_color(LED_BLUE);
    vTaskDelay(pdMS_TO_TICKS(1000));

    led_set_color(LED_WHITE);
    vTaskDelay(pdMS_TO_TICKS(1000));

    led_set_color(LED_OFF);
}
