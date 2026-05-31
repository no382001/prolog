#pragma once
#include "hardware/spi.h"
#include <stdint.h>

#define ILI_SCK   18
#define ILI_MOSI  19
#define ILI_CS    17
#define ILI_DC    20
#define ILI_RST   21

#define ILI_W     320
#define ILI_H     240
#define CHAR_W    6    /* 5px font + 1px gap */
#define CHAR_H    9    /* 8px font + 1px gap */
#define GRID_COLS (ILI_W / CHAR_W)   /* 53 */
#define GRID_ROWS (ILI_H / CHAR_H)   /* 26 */

/* RGB565 palette */
#define ILI_BLACK   0x0000
#define ILI_WHITE   0xFFFF
#define ILI_RED     0xF800
#define ILI_GREEN   0x07E0
#define ILI_BLUE    0x001F
#define ILI_CYAN    0x07FF
#define ILI_YELLOW  0xFFE0
#define ILI_GRAY    0x8410
#define ILI_DKBLUE  0x000F
#define ILI_DKGREEN 0x0320
#define ILI_DKGRAY  0x2104

void     ili9341_init(void);
void     ili9341_fill(uint16_t color);
void     ili9341_draw_row(int row, const char *text, uint16_t fg, uint16_t bg);
