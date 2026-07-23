#pragma once
#include <stdint.h>

#define LCD_RS  12
#define LCD_E   11
#define LCD_D4  10
#define LCD_D5  9
#define LCD_D6  8
#define LCD_D7  7
#define LCD_COLS 16
#define LCD_ROWS 2

void hd44780_init(void);
void hd44780_clear(void);
void hd44780_puts(uint8_t row, const char *s);
