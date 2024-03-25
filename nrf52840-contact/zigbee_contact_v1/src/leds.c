#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>

#include "leds.h"

#define LEDS_NODE DT_PATH(leds)

#define GPIO_SPEC_AND_COMMA(button_or_led) GPIO_DT_SPEC_GET(button_or_led, gpios),

static const struct gpio_dt_spec leds[] = {
#if DT_NODE_EXISTS(LEDS_NODE)
	DT_FOREACH_CHILD(LEDS_NODE, GPIO_SPEC_AND_COMMA)
#endif
};

void led_init (void)
{
	for (size_t i = 0; i < ARRAY_SIZE(leds); i++) {
		gpio_pin_configure_dt(&leds[i], GPIO_OUTPUT);
	}
}

void led_set (uint8_t led_idx, uint32_t val)
{
	if (led_idx >= ARRAY_SIZE(leds)) {
		return;
	}
	gpio_pin_set_dt(&leds[led_idx], val);
}

void led_set_on (uint8_t led_idx)
{
	led_set (led_idx, 1);
}

void led_set_off (uint8_t led_idx)
{
	led_set (led_idx, 0);
}
