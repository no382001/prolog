#include "prolog.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static void default_write_str(prolog_ctx_t *ctx, const char *str, void *userdata) {
  (void)ctx;
  (void)userdata;
  printf("%s", str);
}

static void default_write_term(prolog_ctx_t *ctx, term_t *t, env_t *env, void *userdata) {
  (void)userdata;
  print_term(ctx, t, env);
}

static void default_writef(prolog_ctx_t *ctx, const char *fmt, va_list args, void *userdata) {
  (void)ctx;
  (void)userdata;
  vprintf(fmt, args);
}

static int default_read_char(prolog_ctx_t *ctx, void *userdata) {
  (void)ctx;
  (void)userdata;
  return getchar();
}

static char* default_read_line(prolog_ctx_t *ctx, char *buf, int size, void *userdata) {
  (void)ctx;
  (void)userdata;
  return fgets(buf, size, stdin);
}

void io_hooks_init_default(prolog_ctx_t *ctx) {
  ctx->io_hooks.write_str = default_write_str;
  ctx->io_hooks.write_term = default_write_term;
  ctx->io_hooks.writef = default_writef;
  ctx->io_hooks.read_char = default_read_char;
  ctx->io_hooks.read_line = default_read_line;
  ctx->io_hooks.userdata = NULL;
}

void io_hooks_set(prolog_ctx_t *ctx, io_hooks_t *hooks) {
  if (hooks->write_str)
    ctx->io_hooks.write_str = hooks->write_str;
  if (hooks->write_term)
    ctx->io_hooks.write_term = hooks->write_term;
  if (hooks->writef)
    ctx->io_hooks.writef = hooks->writef;
  if (hooks->read_char)
    ctx->io_hooks.read_char = hooks->read_char;
  if (hooks->read_line)
    ctx->io_hooks.read_line = hooks->read_line;

    ctx->io_hooks.userdata = hooks->userdata;
}

void io_write_str(prolog_ctx_t *ctx, const char *str) {
  if (ctx->io_hooks.write_str) {
    ctx->io_hooks.write_str(ctx, str, ctx->io_hooks.userdata);
  }
}

void io_write_term(prolog_ctx_t *ctx, term_t *t, env_t *env) {
  if (ctx->io_hooks.write_term) {
    ctx->io_hooks.write_term(ctx, t, env, ctx->io_hooks.userdata);
  }
}

void io_writef(prolog_ctx_t *ctx, const char *fmt, ...) {
  if (ctx->io_hooks.writef) {
    va_list args;
    va_start(args, fmt);
    ctx->io_hooks.writef(ctx, fmt, args, ctx->io_hooks.userdata);
    va_end(args);
  }
}

int io_read_char(prolog_ctx_t *ctx) {
  if (ctx->io_hooks.read_char) {
    return ctx->io_hooks.read_char(ctx, ctx->io_hooks.userdata);
  }
  return EOF;
}

char* io_read_line(prolog_ctx_t *ctx, char *buf, int size) {
  if (ctx->io_hooks.read_line) {
    return ctx->io_hooks.read_line(ctx, buf, size, ctx->io_hooks.userdata);
  }
  return NULL;
}
