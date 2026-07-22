#include "platform_impl.h"

bool unify(trilog_ctx_t *ctx, term_t *a, term_t *b, env_t *env) {
  assert(ctx != NULL && "Context is NULL");
  assert(env != NULL && "Environment is NULL");

  ctx->stats.unify_calls++;

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
    ctx->stats.unify_fails++;
    return false;
  }

  if (a->type == VAR) {
    bind(ctx, env, a, b);
    debug(ctx, "  -> OK (bind var a)\n");
    return true;
  }
  if (b->type == VAR) {
    bind(ctx, env, b, a);
    debug(ctx, "  -> OK (bind var b)\n");
    return true;
  }

  // STR (packed char list) handling
  if (a->type == STR && b->type == STR) {
    bool result =
        a->arity == b->arity &&
        (a->name == b->name || memcmp(a->name, b->name, a->arity) == 0);
    return result;
  }
  if (a->type == STR && a->arity == 0 && is_nil(b))
    return true;
  if (b->type == STR && b->arity == 0 && is_nil(a))
    return true;
  if (a->type == STR && a->arity > 0 && b->type == FUNC && is_cons(b)) {
    return unify(ctx, list_head(ctx, a), b->args[0], env) &&
           unify(ctx, list_tail(ctx, a), b->args[1], env);
  }
  if (b->type == STR && b->arity > 0 && a->type == FUNC && is_cons(a)) {
    return unify(ctx, a->args[0], list_head(ctx, b), env) &&
           unify(ctx, a->args[1], list_tail(ctx, b), env);
  }

  if ((a->type == CONST || a->type == INT) &&
      (b->type == CONST || b->type == INT)) {
    // INT and CONST with same name unify (e.g. '1' unifies with 1)
    bool result = a->name == b->name;
    debug(ctx, "  -> %s (const=%s vs %s)\n", result ? "OK" : "FAIL", a->name,
          b->name);
    return result;
  }

  // FLOAT unifies only with an identical-valued FLOAT — never with INT or
  // CONST (ISO requires 1.0 \= 1), unlike the INT/CONST cross-unification
  // above.
  if (a->type == FLOAT && b->type == FLOAT)
    return a->name == b->name;

  if (a->type == FUNC && b->type == FUNC) {
    if (a->name != b->name || a->arity != b->arity) {
      debug(ctx, "  -> FAIL (func mismatch: %s/%d vs %s/%d)\n", a->name,
            a->arity, b->name, b->arity);
      ctx->stats.unify_fails++;
      return false;
    }
    debug(ctx, "  -> checking %d args of %s\n", a->arity, a->name);
    for (int i = 0; i < a->arity; i++) {
      if (!unify(ctx, a->args[i], b->args[i], env)) {
        debug(ctx, "  -> FAIL at arg %d\n", i);
        ctx->stats.unify_fails++;
        return false;
      }
    }
    debug(ctx, "  -> OK (all args)\n");
    return true;
  }

  debug(ctx, "  -> FAIL (type mismatch: %d vs %d)\n", a->type, b->type);
  ctx->stats.unify_fails++;
  return false;
}