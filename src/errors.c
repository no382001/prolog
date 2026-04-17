#include "platform_impl.h"

//****
//* iso error term construction
//****

// write a simple term to a fixed buffer (used for runtime_error strings).
// handles atoms, integers, and functor/N up to 2 levels deep.
static void term_to_buf(const term_t *t, char *buf, int sz) {
  if (!t || sz <= 0)
    return;
  if (t->type == CONST || t->type == INT) {
    snprintf(buf, sz, "%s", t->name);
    return;
  }
  if (t->type == STR) {
    snprintf(buf, sz, "\"%.*s\"", t->arity, t->name);
    return;
  }
  if (t->type == FUNC) {
    // write f/n indicators in infix form
    if (strcmp(t->name, "/") == 0 && t->arity == 2) {
      char a[64], b[64];
      term_to_buf(t->args[0], a, sizeof(a));
      term_to_buf(t->args[1], b, sizeof(b));
      snprintf(buf, sz, "%s/%s", a, b);
      return;
    }
    int pos = snprintf(buf, sz, "%s(", t->name);
    for (int i = 0; i < t->arity && pos < sz - 1; i++) {
      if (i > 0) {
        buf[pos++] = ',';
        if (pos < sz - 1)
          buf[pos++] = ' ';
      }
      char sub[128];
      term_to_buf(t->args[i], sub, sizeof(sub));
      pos += snprintf(buf + pos, sz - pos, "%s", sub);
    }
    if (pos < sz - 1)
      buf[pos++] = ')';
    buf[pos] = '\0';
    return;
  }
  snprintf(buf, sz, "_");
}

void throw_error(trilog_ctx_t *ctx, term_t *error_type, const char *context) {
  term_t *ctx_atom = make_const(ctx, context);
  term_t *args[2] = {error_type, ctx_atom};
  term_t *err = make_func(ctx, "error", args, 2);
  ctx->thrown_ball = err;
  ctx->has_runtime_error = true;
  char type_str[MAX_ERROR_MSG / 2];
  term_to_buf(error_type, type_str, sizeof(type_str));
  snprintf(ctx->runtime_error, MAX_ERROR_MSG, "%s in %s", type_str, context);
}

void throw_instantiation_error(trilog_ctx_t *ctx, const char *context) {
  throw_error(ctx, make_const(ctx, "instantiation_error"), context);
}

void throw_type_error(trilog_ctx_t *ctx, const char *expected, term_t *got,
                      const char *context) {
  term_t *targs[2] = {make_const(ctx, expected), got};
  term_t *te = make_func(ctx, "type_error", targs, 2);
  throw_error(ctx, te, context);
}

void throw_evaluation_error(trilog_ctx_t *ctx, const char *kind,
                            const char *context) {
  term_t *kargs[1] = {make_const(ctx, kind)};
  term_t *ee = make_func(ctx, "evaluation_error", kargs, 1);
  throw_error(ctx, ee, context);
}

void throw_evaluable_error(trilog_ctx_t *ctx, const char *name, int arity,
                           const char *context) {
  term_t *slash_args[2] = {make_const(ctx, name), make_int(ctx, arity)};
  term_t *indicator = make_func(ctx, "/", slash_args, 2);
  term_t *targs[2] = {make_const(ctx, "evaluable"), indicator};
  term_t *te = make_func(ctx, "type_error", targs, 2);
  term_t *ctx_atom = make_const(ctx, context);
  term_t *err_args[2] = {te, ctx_atom};
  ctx->thrown_ball = make_func(ctx, "error", err_args, 2);
  ctx->has_runtime_error = true;
  snprintf(ctx->runtime_error, MAX_ERROR_MSG, "type_error(evaluable, %s/%d)",
           name, arity);
}

void throw_permission_error(trilog_ctx_t *ctx, const char *operation,
                            const char *object_type, term_t *object,
                            const char *context) {
  term_t *pargs[3] = {make_const(ctx, operation), make_const(ctx, object_type),
                      object};
  term_t *pe = make_func(ctx, "permission_error", pargs, 3);
  throw_error(ctx, pe, context);
}

void throw_existence_error(trilog_ctx_t *ctx, const char *object_type,
                           term_t *object, const char *context) {
  term_t *eargs[2] = {make_const(ctx, object_type), object};
  term_t *ee = make_func(ctx, "existence_error", eargs, 2);
  throw_error(ctx, ee, context);
}

void ctx_runtime_error(trilog_ctx_t *ctx, const char *fmt, ...) {
  if (ctx->has_runtime_error)
    return; // keep first error
  ctx->has_runtime_error = true;
  va_list args;
  va_start(args, fmt);
  vsnprintf(ctx->runtime_error, MAX_ERROR_MSG, fmt, args);
  va_end(args);
}
