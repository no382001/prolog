#include "platform_impl.h"

void debug(prolog_ctx_t *ctx, const char *fmt, ...) {
  if (!ctx->debug_enabled)
    return;
  va_list args;
  va_start(args, fmt);
  if (ctx->io_hooks.writef) {
    ctx->io_hooks.writef(ctx, fmt, args, ctx->io_hooks.userdata);
  }
  va_end(args);
}

void print_term_raw(term_t *t) {
  if (!t) {
    printf("NULL");
    return;
  }

  switch (t->type) {
  case CONST:
    printf("CONST(%s)", t->name);
    break;
  case VAR:
    printf("VAR(%s)", t->name);
    break;
  case STRING:
    printf("STRING(\"%s\")", t->string_data);
    break;
  case FUNC:
    printf("FUNC(%s,%d,[", t->name, t->arity);
    for (int i = 0; i < t->arity; i++) {
      if (i > 0)
        printf(",");
      print_term_raw(t->args[i]);
    }
    printf("])");
    break;
  default:
    assert(false && "Invalid term type");
  }
}

void debug_term_raw(prolog_ctx_t *ctx, term_t *t) {
  if (!ctx->debug_enabled)
    return;
  print_term_raw(t);
}