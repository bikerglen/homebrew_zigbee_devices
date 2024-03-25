#ifndef __LEDS_H__
#define __LEDS_H__

#include <zephyr/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*button_handler_t)(uint32_t button_state, uint32_t has_changed);

void buttons_init (button_handler_t button_handler);
void dk_read_buttons (uint32_t *button_state, uint32_t *has_changed);

// TODO

#ifdef __cplusplus
}
#endif

#endif