#include "pico/stdlib.h"
#include "hd44780.h"
#include "leds.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/trilog.h"
#include "core_embed.h"

static uint8_t ctx_buf[TRILOG_CTX_SIZE(TERM_POOL_BYTES)];
static trilog_ctx_t *g_ctx;

static char g_out[512];
static int  g_out_pos;

/* --- display helpers --- */

static void disp_update(const char *row0, const char *row1) {
    hd44780_puts(0, row0);
    hd44780_puts(1, row1);
}

static void disp_stats(const char *label) {
    trilog_usage_t u = trilog_get_usage(g_ctx);
    int hp = u.term_pool_total ? u.term_pool_used * 100 / u.term_pool_total : 0;
    char row1[LCD_COLS + 1];
    snprintf(row1, sizeof(row1), "%-9.9s H:%3d%%", label, hp);
    hd44780_puts(1, row1);
}

/* --- I/O hooks --- */

static void rp2040_write_str(trilog_ctx_t *ctx, const char *s, void *ud) {
    (void)ctx; (void)ud;
    printf("%s", s);
    stdio_flush();
    int len = (int)strlen(s);
    int rem = (int)sizeof(g_out) - 1 - g_out_pos;
    if (len > rem) len = rem;
    memcpy(g_out + g_out_pos, s, (size_t)len);
    g_out_pos += len;
    g_out[g_out_pos] = '\0';
}

static void rp2040_writef(trilog_ctx_t *ctx, const char *fmt, va_list ap, void *ud) {
    char tmp[256];
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    rp2040_write_str(ctx, tmp, ud);
}

static char *rp2040_read_line(trilog_ctx_t *ctx, char *buf, int size, void *ud) {
    (void)ctx; (void)ud;
    int i = 0, c = 0;
    while (i < size - 1) {
        c = getchar();
        if (c == '\n' || c == EOF) break;
        if (c == '\r') { putchar('\n'); stdio_flush(); break; }
        if (c == 127 || c == '\b') {
            if (i > 0) { i--; printf("\b \b"); stdio_flush(); }
            continue;
        }
        buf[i++] = (char)c; putchar(c); stdio_flush();
    }
    buf[i] = '\0';
    putchar('\n'); stdio_flush();
    return (i == 0 && c == EOF) ? NULL : buf;
}

static int      rp2040_read_char(trilog_ctx_t *ctx, void *ud)              { (void)ctx;(void)ud; return getchar(); }
static bool     rp2040_file_exists(trilog_ctx_t *ctx, const char *p, void *ud) { (void)ctx;(void)p;(void)ud; return false; }
static long long rp2040_file_mtime(trilog_ctx_t *ctx, const char *p, void *ud)  { (void)ctx;(void)p;(void)ud; return -1; }
static double   rp2040_clock(trilog_ctx_t *ctx, void *ud)                  { (void)ctx;(void)ud; return (double)time_us_64()/1e6; }

int main(void) {
    stdio_init_all();
    leds_init();
    hd44780_init();

    disp_update("trilog", "M0+ RP2040 264K");

    g_ctx = (trilog_ctx_t *)ctx_buf;
    trilog_ctx_init(g_ctx, TERM_POOL_BYTES);
    ops_init_defaults(g_ctx);

    io_hooks_init_default(g_ctx);
    io_hooks_t hooks = {0};
    hooks.write_str       = rp2040_write_str;
    hooks.writef          = rp2040_writef;
    hooks.writef_err      = rp2040_writef;
    hooks.read_line       = rp2040_read_line;
    hooks.read_char       = rp2040_read_char;
    hooks.file_exists     = rp2040_file_exists;
    hooks.file_mtime      = rp2040_file_mtime;
    hooks.clock_monotonic = rp2040_clock;
    io_hooks_set(g_ctx, &hooks);

    char *core_str = malloc(core_pl_len + 1);
    if (core_str) {
        memcpy(core_str, core_pl, core_pl_len);
        core_str[core_pl_len] = '\0';
        trilog_load_string(g_ctx, core_str);
        free(core_str);
    }

    leds_register_ffi(g_ctx);

    disp_update("trilog ready", "");

    char line[256];
    while (1) {
        io_write_str(g_ctx, "?- ");
        disp_stats("ready");
        if (!io_read_line(g_ctx, line, sizeof(line))) break;
        if (!strlen(line)) continue;
        if (!strcmp(line, "halt.")) break;

        /* show truncated query on row 0 */
        char row0[LCD_COLS + 1];
        snprintf(row0, sizeof(row0), "?-%.14s", line);
        hd44780_puts(0, row0);
        hd44780_puts(1, "running...      ");

        g_out_pos = 0; g_out[0] = '\0';
        exec_query_interactive(g_ctx, line);

        /* show first line of output on row 1 */
        char result[LCD_COLS + 1] = "done";
        if (g_out_pos > 0) {
            const char *nl = strchr(g_out, '\n');
            int len = nl ? (int)(nl - g_out) : g_out_pos;
            if (len >= LCD_COLS) len = LCD_COLS - 1;
            memcpy(result, g_out, (size_t)len);
            result[len] = '\0';
        }
        disp_stats(result);
    }

    disp_update("halted.", "");
    return 0;
}
