#pragma once
#include <stdint.h>
#include "../src/trilog.h"

#define LED_COUNT 5
extern const uint8_t led_pins[LED_COUNT];

void leds_init(void);
void leds_register_ffi(trilog_ctx_t *ctx);
