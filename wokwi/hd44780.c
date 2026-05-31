#include "hd44780.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"

static void pulse_e(void) {
    gpio_put(LCD_E, 1); sleep_us(1);
    gpio_put(LCD_E, 0); sleep_us(50);
}

static void write_nibble(uint8_t n) {
    gpio_put(LCD_D4, (n >> 0) & 1);
    gpio_put(LCD_D5, (n >> 1) & 1);
    gpio_put(LCD_D6, (n >> 2) & 1);
    gpio_put(LCD_D7, (n >> 3) & 1);
    pulse_e();
}

static void send(uint8_t rs, uint8_t byte) {
    gpio_put(LCD_RS, rs);
    write_nibble(byte >> 4);
    write_nibble(byte & 0xF);
}

void hd44780_init(void) {
    const uint8_t pins[] = {LCD_RS, LCD_E, LCD_D4, LCD_D5, LCD_D6, LCD_D7};
    for (int i = 0; i < 6; i++) {
        gpio_init(pins[i]);
        gpio_set_dir(pins[i], GPIO_OUT);
        gpio_put(pins[i], 0);
    }

    sleep_ms(50);
    gpio_put(LCD_RS, 0);
    write_nibble(0x3); sleep_ms(5);
    write_nibble(0x3); sleep_us(200);
    write_nibble(0x3); sleep_us(200);
    write_nibble(0x2); sleep_us(200); /* switch to 4-bit */

    send(0, 0x28); /* 2 lines, 5x8 font */
    send(0, 0x0C); /* display on, cursor off */
    send(0, 0x06); /* auto-increment, no shift */
    send(0, 0x01); /* clear */
    sleep_ms(2);
}

void hd44780_clear(void) {
    send(0, 0x01);
    sleep_ms(2);
}

void hd44780_puts(uint8_t row, const char *s) {
    send(0, 0x80 | (row ? 0x40 : 0x00));
    for (int i = 0; i < LCD_COLS; i++)
        send(1, *s ? (uint8_t)*s++ : ' ');
}
