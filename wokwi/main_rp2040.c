#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "ssd1306.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/trilog.h"
#include "core_embed.h"

#define I2C_SDA 4
#define I2C_SCL 5

#define DISP_COLS SSD1306_COLS  /* 21 */
#define OUT_LINES 2

static uint8_t ctx_buf[TRILOG_CTX_SIZE(TERM_POOL_BYTES)];
static trilog_ctx_t *g_ctx;

/* --- output capture for display --- */
static char g_outbuf[512];
static int  g_outpos;

static char disp_query[DISP_COLS + 1];
static char disp_out[OUT_LINES][DISP_COLS + 1];

/* --- I/O hooks --- */

static void rp2040_write_str(trilog_ctx_t *ctx, const char *s, void *ud) {
    (void)ctx; (void)ud;
    printf("%s", s);
    stdio_flush();
    int len = (int)strlen(s);
    int rem = (int)sizeof(g_outbuf) - 1 - g_outpos;
    if (len > rem) len = rem;
    memcpy(g_outbuf + g_outpos, s, (size_t)len);
    g_outpos += len;
    g_outbuf[g_outpos] = '\0';
}

static void rp2040_writef(trilog_ctx_t *ctx, const char *fmt, va_list ap, void *ud) {
    (void)ctx; (void)ud;
    char tmp[256];
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    rp2040_write_str(ctx, tmp, ud);
}

static char *rp2040_read_line(trilog_ctx_t *ctx, char *buf, int size, void *ud) {
    (void)ctx; (void)ud;
    int i = 0, c;
    while (i < size - 1) {
        c = getchar();
        if (c == '\n' || c == EOF) break;
        if (c == '\r') { putchar('\n'); stdio_flush(); break; }
        if (c == 127 || c == '\b') {
            if (i > 0) { i--; printf("\b \b"); stdio_flush(); }
            continue;
        }
        buf[i++] = (char)c;
        putchar(c);
        stdio_flush();
    }
    buf[i] = '\0';
    putchar('\n');
    stdio_flush();
    return (i == 0 && c == EOF) ? NULL : buf;
}

static int rp2040_read_char(trilog_ctx_t *ctx, void *ud) {
    (void)ctx; (void)ud;
    return getchar();
}

static bool rp2040_file_exists(trilog_ctx_t *ctx, const char *path, void *ud) {
    (void)ctx; (void)path; (void)ud;
    return false;
}

static long long rp2040_file_mtime(trilog_ctx_t *ctx, const char *path, void *ud) {
    (void)ctx; (void)path; (void)ud;
    return -1;
}

static double rp2040_clock(trilog_ctx_t *ctx, void *ud) {
    (void)ctx; (void)ud;
    return (double)time_us_64() / 1e6;
}

/* --- display update --- */

static void truncate_copy(char *dst, const char *src, int max) {
    int len = (int)strlen(src);
    if (len <= max) {
        strncpy(dst, src, (size_t)max);
        dst[max] = '\0';
    } else {
        memcpy(dst, src, (size_t)(max - 1));
        dst[max - 1] = '.';
        dst[max] = '\0';
    }
}

static void disp_parse_output(void) {
    memset(disp_out, ' ', sizeof(disp_out));
    for (int i = 0; i < OUT_LINES; i++) disp_out[i][DISP_COLS] = '\0';

    int line = 0;
    char *p = g_outbuf;
    while (*p && line < OUT_LINES) {
        char *nl = strchr(p, '\n');
        int len = nl ? (int)(nl - p) : (int)strlen(p);
        if (len > 0) {
            truncate_copy(disp_out[line], p, DISP_COLS);
            line++;
        }
        if (!nl) break;
        p = nl + 1;
    }
}

static void disp_bar(char *buf, int used, int total, int bar_w) {
    int filled = total > 0 ? (used * bar_w / total) : 0;
    if (filled > bar_w) filled = bar_w;
    buf[0] = '[';
    for (int i = 0; i < bar_w; i++) buf[1 + i] = (i < filled) ? '#' : ' ';
    buf[1 + bar_w] = ']';
    buf[2 + bar_w] = '\0';
}

static void disp_update(void) {
    trilog_usage_t u = trilog_get_usage(g_ctx);

    ssd1306_clear();

    /* row 0: header */
    ssd1306_puts(0, 0, "trilog  M0+ 264K");

    /* row 1: last query */
    char qline[DISP_COLS + 4];
    snprintf(qline, sizeof(qline), "?-%s", disp_query);
    ssd1306_puts(1, 0, qline);

    /* rows 2-3: output */
    disp_parse_output();
    ssd1306_puts(2, 0, disp_out[0]);
    ssd1306_puts(3, 0, disp_out[1]);

    /* row 4: separator */
    ssd1306_hline(4);

    /* row 5: heap bar  used/total */
    char bar[12];
    disp_bar(bar, u.term_pool_used, u.term_pool_total, 7);
    char row5[DISP_COLS + 1];
    int hp = u.term_pool_total > 0
        ? (u.term_pool_used * 100 / u.term_pool_total) : 0;
    snprintf(row5, sizeof(row5), "heap%s%3d%%", bar, hp);
    ssd1306_puts(5, 0, row5);

    /* row 6: str + cls */
    char row6[DISP_COLS + 1];
    int sp = u.string_pool_total > 0
        ? (u.string_pool_used * 100 / u.string_pool_total) : 0;
    snprintf(row6, sizeof(row6), "str:%3d%% cls:%3d", sp, u.clauses_used);
    ssd1306_puts(6, 0, row6);

    /* row 7: inferences */
    char row7[DISP_COLS + 1];
    int infer = g_ctx->stats.son_calls;
    if (infer >= 1000000)
        snprintf(row7, sizeof(row7), "infer:%dM", infer / 1000000);
    else if (infer >= 1000)
        snprintf(row7, sizeof(row7), "infer:%dK", infer / 1000);
    else
        snprintf(row7, sizeof(row7), "infer:%d", infer);
    ssd1306_puts(7, 0, row7);

    ssd1306_flush();
}

int main(void) {
    stdio_init_all();

    /* I2C for OLED */
    i2c_init(i2c0, 400000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    ssd1306_init(i2c0);

    /* trilog init */
    g_ctx = (trilog_ctx_t *)ctx_buf;
    trilog_ctx_init(g_ctx, TERM_POOL_BYTES);
    ops_init_defaults(g_ctx);

    io_hooks_init_default(g_ctx);
    io_hooks_t hooks = {0};
    hooks.write_str   = rp2040_write_str;
    hooks.writef      = rp2040_writef;
    hooks.writef_err  = rp2040_writef;
    hooks.read_line   = rp2040_read_line;
    hooks.read_char   = rp2040_read_char;
    hooks.file_exists     = rp2040_file_exists;
    hooks.file_mtime      = rp2040_file_mtime;
    hooks.clock_monotonic = rp2040_clock;
    io_hooks_set(g_ctx, &hooks);

    /* load core.pl from flash */
    char *core_str = malloc(core_pl_len + 1);
    if (core_str) {
        memcpy(core_str, core_pl, core_pl_len);
        core_str[core_pl_len] = '\0';
        trilog_load_string(g_ctx, core_str);
        free(core_str);
    }

    ssd1306_puts(0, 0, "trilog  M0+ 264K");
    ssd1306_puts(1, 0, "ready.");
    ssd1306_flush();

    char line[256];
    while (1) {
        io_write_str(g_ctx, "?- ");
        if (!io_read_line(g_ctx, line, sizeof(line)))
            break;
        if (strlen(line) == 0) continue;
        if (strcmp(line, "halt.") == 0) break;

        truncate_copy(disp_query, line, DISP_COLS - 2);
        g_outpos = 0;
        g_outbuf[0] = '\0';

        exec_query_interactive(g_ctx, line);

        disp_update();
    }

    ssd1306_clear();
    ssd1306_puts(3, 3, "halted.");
    ssd1306_flush();
    return 0;
}
