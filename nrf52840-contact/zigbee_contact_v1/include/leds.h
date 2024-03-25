#ifndef __LEDS_H__
#define __LEDS_H__

#include <zephyr/types.h>

#ifdef __cplusplus
extern "C" {
#endif

void led_init (void);
void led_set (uint8_t led_idx, uint32_t val);
void led_set_on (uint8_t led_idx);
void led_set_off (uint8_t led_idx);

#ifdef __cplusplus
}
#endif

#endif