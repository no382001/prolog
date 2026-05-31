#pragma once
#include "hardware/i2c.h"
#include <stdint.h>

#define SSD1306_W    128
#define SSD1306_H    64
#define SSD1306_PAGES (SSD1306_H / 8)
#define SSD1306_ADDR  0x3C
#define SSD1306_COLS  (SSD1306_W / 6)  // 21 chars per row (6px wide each)

void ssd1306_init(i2c_inst_t *i2c);
void ssd1306_clear(void);
void ssd1306_puts(int row, int col, const char *s);
void ssd1306_hline(int row);
void ssd1306_flush(void);
