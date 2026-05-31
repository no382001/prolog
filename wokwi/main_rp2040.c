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

/* --- LCD FFI ---
   lcd_clear            : clear display
   lcd_row(+Row, +Text) : write string to row 0 or 1  */

static builtin_result_t b_lcd_clear(trilog_ctx_t *ctx, term_t *goal, env_t *env) {
    (void)ctx; (void)goal; (void)env;
    hd44780_clear();
    return BUILTIN_OK;
}

static builtin_result_t b_lcd_row(trilog_ctx_t *ctx, term_t *goal, env_t *env) {
    (void)ctx;
    term_t *r = deref(env, goal->args[0]);
    term_t *t = deref(env, goal->args[1]);
    long row = strtol(r->name, NULL, 10);
    if (row < 0 || row >= LCD_ROWS) return BUILTIN_FAIL;
    hd44780_puts((uint8_t)row, t->name);
    return BUILTIN_OK;
}

/* --- I/O hooks --- */

static void rp2040_write_str(trilog_ctx_t *ctx, const char *s, void *ud) {
    (void)ctx; (void)ud;
    printf("%s", s);
    stdio_flush();
}

static void rp2040_writef(trilog_ctx_t *ctx, const char *fmt, va_list ap, void *ud) {
    (void)ctx; (void)ud;
    vprintf(fmt, ap);
    stdio_flush();
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
    //putchar('\n');
    stdio_flush();
    return (i == 0 && c == EOF) ? NULL : buf;
}

static int       rp2040_read_char(trilog_ctx_t *ctx, void *ud)                  { (void)ctx;(void)ud; return getchar(); }
static bool      rp2040_file_exists(trilog_ctx_t *ctx, const char *p, void *ud) { (void)ctx;(void)p;(void)ud; return false; }
static long long rp2040_file_mtime(trilog_ctx_t *ctx, const char *p, void *ud)  { (void)ctx;(void)p;(void)ud; return -1; }
static double    rp2040_clock(trilog_ctx_t *ctx, void *ud)                      { (void)ctx;(void)ud; return (double)time_us_64()/1e6; }

int main(void) {
    stdio_init_all();
    leds_init();
    hd44780_init();
    hd44780_puts(0, "trilog");

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
    ffi_register_builtin(g_ctx, "lcd_clear", 0, b_lcd_clear, NULL);
    ffi_register_builtin(g_ctx, "lcd_row",   2, b_lcd_row,   NULL);

    char line[256];
    while (1) {
        io_write_str(g_ctx, "?- ");
        if (!io_read_line(g_ctx, line, sizeof(line))) break;
        if (!strlen(line)) continue;
        if (!strcmp(line, "halt.")) break;
        exec_query_interactive(g_ctx, line);
    }

    hd44780_puts(0, "halted.");
    hd44780_puts(1, "");
    return 0;
}
