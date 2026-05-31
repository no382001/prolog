#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "ili9341.h"
#include "leds.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/trilog.h"
#include "core_embed.h"

/* --- display history ---
   Row 0           : header (drawn once at startup)
   Rows 1..HIST_N  : scrolling REPL output
   Row GRID_ROWS-3 : separator
   Row GRID_ROWS-2 : heap / string stats
   Row GRID_ROWS-1 : cls / binds / infer stats              */

#define HIST_N (GRID_ROWS - 4)   /* 22 history lines */

static char     hist[HIST_N][GRID_COLS + 1];
static uint16_t hist_fg[HIST_N];
static int      hist_n = 0;

static void hist_push(const char *line, uint16_t color) {
    if (hist_n == HIST_N) {
        memmove(hist,    hist    + 1, (HIST_N - 1) * sizeof(hist[0]));
        memmove(hist_fg, hist_fg + 1, (HIST_N - 1) * sizeof(hist_fg[0]));
        hist_n--;
    }
    strncpy(hist[hist_n], line, GRID_COLS);
    hist[hist_n][GRID_COLS] = '\0';
    hist_fg[hist_n] = color;
    hist_n++;
}

/* Push multi-line output from a buffer into history. */
static void hist_push_output(const char *buf, uint16_t color) {
    const char *p = buf;
    char line[GRID_COLS + 1];
    while (*p) {
        const char *nl = strchr(p, '\n');
        int len = nl ? (int)(nl - p) : (int)strlen(p);
        if (len > 0) {
            int take = len < GRID_COLS ? len : GRID_COLS;
            memcpy(line, p, (size_t)take);
            line[take] = '\0';
            hist_push(line, color);
        }
        if (!nl) break;
        p = nl + 1;
    }
}

static void disp_redraw_history(void) {
    for (int i = 0; i < HIST_N; i++) {
        const char *text = (i < hist_n) ? hist[i] : "";
        uint16_t    fg   = (i < hist_n) ? hist_fg[i] : ILI_BLACK;
        ili9341_draw_row(1 + i, text, fg, ILI_BLACK);
    }
}

static void disp_update_stats(trilog_ctx_t *ctx) {
    trilog_usage_t u = trilog_get_usage(ctx);

    /* separator */
    char sep[GRID_COLS + 1];
    memset(sep, '-', GRID_COLS); sep[GRID_COLS] = '\0';
    ili9341_draw_row(GRID_ROWS - 3, sep, ILI_DKGRAY, ILI_BLACK);

    /* row: heap [####   ]52%  str [##     ]15% */
    auto void bar(char *dst, int used, int total, int w) {
        int f = total > 0 ? (used * w / total) : 0;
        *dst++ = '[';
        for (int i = 0; i < w; i++) *dst++ = (i < f) ? '#' : ' ';
        *dst++ = ']'; *dst = '\0';
    }
    char b1[12], b2[12], row[GRID_COLS + 1];
    int hp = u.term_pool_total   ? u.term_pool_used   * 100 / u.term_pool_total   : 0;
    int sp = u.string_pool_total ? u.string_pool_used * 100 / u.string_pool_total : 0;
    bar(b1, u.term_pool_used,   u.term_pool_total,   8);
    bar(b2, u.string_pool_used, u.string_pool_total, 8);
    snprintf(row, sizeof(row), "heap%s%3d%%  str%s%3d%%", b1, hp, b2, sp);
    ili9341_draw_row(GRID_ROWS - 2, row, ILI_GREEN, ILI_BLACK);

    /* row: cls N/256  binds N/1024  infer NM */
    int infer = ctx->stats.son_calls;
    char inf_s[16];
    if      (infer >= 1000000) snprintf(inf_s, sizeof(inf_s), "%dM", infer/1000000);
    else if (infer >= 1000)    snprintf(inf_s, sizeof(inf_s), "%dK", infer/1000);
    else                       snprintf(inf_s, sizeof(inf_s), "%d",  infer);
    snprintf(row, sizeof(row), "cls %d/%d  binds %d/%d  infer %s",
             u.clauses_used, u.clauses_total,
             u.bindings_used, u.bindings_total, inf_s);
    ili9341_draw_row(GRID_ROWS - 1, row, ILI_CYAN, ILI_BLACK);
}

/* --- trilog context and output buffer --- */

static uint8_t ctx_buf[TRILOG_CTX_SIZE(TERM_POOL_BYTES)];
static trilog_ctx_t *g_ctx;

static char g_out[1024];
static int  g_out_pos;

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
        buf[i++] = (char)c;
        putchar(c); stdio_flush();
    }
    buf[i] = '\0';
    putchar('\n'); stdio_flush();
    return (i == 0 && c == EOF) ? NULL : buf;
}

static int      rp2040_read_char(trilog_ctx_t *ctx, void *ud)                        { (void)ctx;(void)ud; return getchar(); }
static bool     rp2040_file_exists(trilog_ctx_t *ctx, const char *p, void *ud)        { (void)ctx;(void)p;(void)ud; return false; }
static long long rp2040_file_mtime(trilog_ctx_t *ctx, const char *p, void *ud)        { (void)ctx;(void)p;(void)ud; return -1; }
static double   rp2040_clock(trilog_ctx_t *ctx, void *ud)                             { (void)ctx;(void)ud; return (double)time_us_64()/1e6; }

int main(void) {
    stdio_init_all();
    leds_init();
    ili9341_init();

    /* header row (drawn once) */
    char hdr[GRID_COLS + 1];
    snprintf(hdr, sizeof(hdr), " trilog  |  M0+ RP2040  264KB SRAM  |  5 LEDs mapped");
    ili9341_draw_row(0, hdr, ILI_WHITE, ILI_DKBLUE);

    /* trilog init */
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

    /* load core.pl from flash */
    char *core_str = malloc(core_pl_len + 1);
    if (core_str) {
        memcpy(core_str, core_pl, core_pl_len);
        core_str[core_pl_len] = '\0';
        trilog_load_string(g_ctx, core_str);
        free(core_str);
    }

    /* register LED predicates */
    leds_register_ffi(g_ctx);

    hist_push("ready. type prolog queries below.", ILI_GRAY);
    hist_push("led(X), led_on(X), sleep_ms(200), led_off(X).", ILI_DKGRAY);
    disp_redraw_history();
    disp_update_stats(g_ctx);

    char line[256];
    while (1) {
        io_write_str(g_ctx, "?- ");
        if (!io_read_line(g_ctx, line, sizeof(line))) break;
        if (!strlen(line)) continue;
        if (!strcmp(line, "halt.")) break;

        /* show query in history */
        char qline[GRID_COLS + 1];
        snprintf(qline, sizeof(qline), "?- %s", line);
        hist_push(qline, ILI_CYAN);
        disp_redraw_history();

        /* run query */
        g_out_pos = 0; g_out[0] = '\0';
        exec_query_interactive(g_ctx, line);

        /* push output into history */
        if (g_out_pos > 0)
            hist_push_output(g_out, ILI_GREEN);

        disp_redraw_history();
        disp_update_stats(g_ctx);
    }

    ili9341_fill(ILI_BLACK);
    ili9341_draw_row(13, "  halted.", ILI_RED, ILI_BLACK);
    return 0;
}
