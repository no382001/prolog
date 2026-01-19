#include "platform_impl.h"

term_t *lookup(env_t *env, const char *name) {
  assert(env != NULL && "Environment is NULL");
  assert(name != NULL && "Name is NULL");

  for (int i = env->count - 1; i >= 0; i--) {
    if (strcmp(env->bindings[i].name, name) == 0) {
      return env->bindings[i].value;
    }
  }
  return NULL;
}

void bind(prolog_ctx_t *ctx, env_t *env, const char *name, term_t *value) {
  assert(ctx != NULL && "Context is NULL");
  assert(env != NULL && "Environment is NULL");
  assert(name != NULL && "Name is NULL");
  assert(value != NULL && "Value is NULL");
  assert(env->count < MAX_BINDINGS && "Binding table full");

  if (ctx->debug_enabled) {
    debug(ctx, "  BIND: %s = ", name);
    debug_term_raw(ctx, value);
    debug(ctx, "\n");
  }

  binding_t *b = &env->bindings[env->count++];
  strncpy(b->name, name, MAX_NAME - 1);
  b->value = value;
}

term_t *deref(env_t *env, term_t *t) {
  assert(env != NULL && "Environment is NULL");

  while (t && t->type == VAR) {
    term_t *val = lookup(env, t->name);
    if (!val)
      break;
    t = val;
  }
  return t;
}

term_t *substitute(prolog_ctx_t *ctx, env_t *env, term_t *t) {
  assert(ctx != NULL && "Context is NULL");
  assert(env != NULL && "Environment is NULL");

  if (!t)
    return NULL;
  t = deref(env, t);

  if (t->type != FUNC)
    return t;

  term_t *args[MAX_ARGS];
  for (int i = 0; i < t->arity; i++) {
    args[i] = substitute(ctx, env, t->args[i]);
  }
  return make_func(ctx, t->name, args, t->arity);
}