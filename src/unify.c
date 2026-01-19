#include "platform_impl.h"

bool unify(prolog_ctx_t *ctx, term_t *a, term_t *b, env_t *env) {
  assert(ctx != NULL && "Context is NULL");
  assert(env != NULL && "Environment is NULL");

  a = deref(env, a);
  b = deref(env, b);

  if (ctx->debug_enabled) {
    debug(ctx, "UNIFY: ");
    debug_term_raw(ctx, a);
    debug(ctx, " WITH ");
    debug_term_raw(ctx, b);
    debug(ctx, "\n");
  }

  if (!a || !b) {
    debug(ctx, "  -> FAIL (null)\n");
    return false;
  }

  if (a->type == VAR) {
    bind(ctx, env, a->name, b);
    debug(ctx, "  -> OK (bind var a)\n");
    return true;
  }
  if (b->type == VAR) {
    bind(ctx, env, b->name, a);
    debug(ctx, "  -> OK (bind var b)\n");
    return true;
  }

  if (a->type == CONST && b->type == CONST) {
    bool result = strcmp(a->name, b->name) == 0;
    debug(ctx, "  -> %s (const=%s vs %s)\n", result ? "OK" : "FAIL", a->name,
          b->name);
    return result;
  }

  if (a->type == STRING && b->type == STRING) {
    bool result = strcmp(a->string_data, b->string_data) == 0;
    debug(ctx, "  -> %s (string=\"%s\" vs \"%s\")\n", result ? "OK" : "FAIL", 
          a->string_data, b->string_data);
    return result;
  }

  if (a->type == FUNC && b->type == FUNC) {
    if (strcmp(a->name, b->name) != 0 || a->arity != b->arity) {
      debug(ctx, "  -> FAIL (func mismatch: %s/%d vs %s/%d)\n", a->name,
            a->arity, b->name, b->arity);
      return false;
    }
    debug(ctx, "  -> checking %d args of %s\n", a->arity, a->name);
    for (int i = 0; i < a->arity; i++) {
      if (!unify(ctx, a->args[i], b->args[i], env)) {
        debug(ctx, "  -> FAIL at arg %d\n", i);
        return false;
      }
    }
    debug(ctx, "  -> OK (all args)\n");
    return true;
  }

  debug(ctx, "  -> FAIL (type mismatch: %d vs %d)\n", a->type, b->type);
  return false;
}