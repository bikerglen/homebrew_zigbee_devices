#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>

#include "buttons.h"

#define BUTTONS_NODE DT_PATH(buttons)

#define GPIO_SPEC_AND_COMMA(button_or_led) GPIO_DT_SPEC_GET(button_or_led, gpios),

static const struct gpio_dt_spec buttons[] = {
#if DT_NODE_EXISTS(BUTTONS_NODE)
	DT_FOREACH_CHILD(BUTTONS_NODE, GPIO_SPEC_AND_COMMA)
#endif
};

static uint32_t buttons_read (void);
static void buttons_changed (const struct device *gpio_dev, struct gpio_callback *cb, uint32_t pins);

static button_handler_t button_handler_cb;
static struct gpio_callback gpio_cb;
static atomic_t buttons_state;

void buttons_init (button_handler_t button_handler)
{
    int err;

    printk (">>> buttons init\n");

    button_handler_cb = button_handler;

    uint32_t pin_mask = 0;

    for (size_t i = 0; i < ARRAY_SIZE(buttons); i++) {
        printk ("initializing button %d\n", i);

        err = gpio_pin_configure_dt(&buttons[i], GPIO_INPUT);
        if (err) {
			printk ("Cannot configure button gpio");
			return err;
		}

		err = gpio_pin_interrupt_configure_dt(&buttons[i], GPIO_INT_TRIG_BOTH);
        if (err) {
			printk ("Cannot enable trig both callback");
			return err;
		}

		pin_mask |= BIT(buttons[i].pin);
	}

    printk ("pin_mask: %08x\n", pin_mask);

	gpio_init_callback (&gpio_cb, buttons_changed, pin_mask);

	for (size_t i = 0; i < ARRAY_SIZE(buttons); i++) {
		gpio_add_callback (buttons[i].port, &gpio_cb);
	}

	atomic_set (&buttons_state, (atomic_val_t)buttons_read ());

    dk_read_buttons (NULL, NULL);

    printk ("<<< buttons init\n");
}

static uint32_t buttons_read (void)
{
	uint32_t ret = 0;
	for (size_t i = 0; i < ARRAY_SIZE(buttons); i++) {
		int val;
		val = gpio_pin_get_dt(&buttons[i]);
		if (val) {
			ret |= 1U << i;
		}
	}
	return ret;
}

static void buttons_changed (const struct device *gpio_dev, struct gpio_callback *cb, uint32_t pins)
{
    printk ("button changed state\n");

    uint32_t last_buttons = atomic_get (&buttons_state);
    uint32_t this_buttons = buttons_read ();
    uint32_t changed_buttons = last_buttons ^ this_buttons;
 	atomic_set (&buttons_state, (atomic_val_t)this_buttons);

    printk ("buttons changed: last: %08x this: %08x changed: %08x\n", last_buttons, this_buttons, changed_buttons);

	button_handler_cb (this_buttons, changed_buttons);
}

void dk_read_buttons (uint32_t *button_state, uint32_t *has_changed)
{
	static uint32_t last_state;
	uint32_t current_state = atomic_get(&buttons_state);

	if (button_state != NULL) {
		*button_state = current_state;
	}

	if (has_changed != NULL) {
		*has_changed = (current_state ^ last_state);
	}

    printk ("dk_read_buttons: last: %08x this: %08x changed: %08x\n", last_state, current_state, *has_changed);

	last_state = current_state;
}

