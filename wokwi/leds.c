#include "leds.h"
#include "rp2040_embed.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include <stdlib.h>
#include <string.h>

/* GP6-GP10: clear of SPI pins (GP17-GP21) and UART (GP0-GP1) */
const uint8_t led_pins[LED_COUNT] = {13, 14, 15, 16, 17};

void leds_init(void) {
    for (int i = 0; i < LED_COUNT; i++) {
        gpio_init(led_pins[i]);
        gpio_set_dir(led_pins[i], GPIO_OUT);
        gpio_put(led_pins[i], 0);
    }
}

static int led_id(trilog_ctx_t *ctx, term_t *goal, env_t *env) {
    (void)ctx;
    term_t *arg = deref(env, goal->args[0]);
    char *end;
    long n = strtol(arg->name, &end, 10);
    if (*end != '\0' || n < 0 || n >= LED_COUNT) return -1;
    return (int)n;
}

static builtin_result_t b_led_on(trilog_ctx_t *ctx, term_t *goal, env_t *env) {
    int n = led_id(ctx, goal, env);
    if (n < 0) return BUILTIN_FAIL;
    gpio_put(led_pins[n], 1);
    return BUILTIN_OK;
}

static builtin_result_t b_led_off(trilog_ctx_t *ctx, term_t *goal, env_t *env) {
    int n = led_id(ctx, goal, env);
    if (n < 0) return BUILTIN_FAIL;
    gpio_put(led_pins[n], 0);
    return BUILTIN_OK;
}

static builtin_result_t b_led_toggle(trilog_ctx_t *ctx, term_t *goal, env_t *env) {
    int n = led_id(ctx, goal, env);
    if (n < 0) return BUILTIN_FAIL;
    gpio_xor_mask(1u << led_pins[n]);
    return BUILTIN_OK;
}

static builtin_result_t b_sleep_ms(trilog_ctx_t *ctx, term_t *goal, env_t *env) {
    (void)ctx;
    term_t *arg = deref(env, goal->args[0]);
    long ms = strtol(arg->name, NULL, 10);
    if (ms > 0) sleep_ms((uint32_t)ms);
    return BUILTIN_OK;
}

void leds_register_ffi(trilog_ctx_t *ctx) {
    /* led/1 facts live in rp2040.pl — loaded here so findall/member work */
    char *src = malloc(rp2040_pl_len + 1);
    if (src) {
        memcpy(src, rp2040_pl, rp2040_pl_len);
        src[rp2040_pl_len] = '\0';
        trilog_load_string(ctx, src);
        free(src);
    }

    ffi_register_builtin(ctx, "led_on",     1, b_led_on,     NULL);
    ffi_register_builtin(ctx, "led_off",    1, b_led_off,    NULL);
    ffi_register_builtin(ctx, "led_toggle", 1, b_led_toggle, NULL);
    ffi_register_builtin(ctx, "sleep_ms",   1, b_sleep_ms,   NULL);
}
