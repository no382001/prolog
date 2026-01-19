#include "platform_impl.h"

void ctx_reset_terms(prolog_ctx_t *ctx) { ctx->term_count = 0; }

term_t *ctx_alloc_term(prolog_ctx_t *ctx) {
  assert(ctx->term_count < MAX_TERMS && "Term pool exhausted");
  term_t *t = &ctx->term_pool[ctx->term_count++];
  memset(t, 0, sizeof(term_t));
  return t;
}

term_t *make_term(prolog_ctx_t *ctx, term_type type, const char *name,
                  term_t **args, int arity) {
  assert(ctx != NULL && "Context is NULL");
  assert(name != NULL && "Term name cannot be NULL");
  assert(arity >= 0 && arity <= MAX_ARGS && "Invalid arity");
  assert(type == CONST || type == VAR || type == FUNC && "Invalid term type");

  term_t *t = ctx_alloc_term(ctx);

  t->type = type;
  strncpy(t->name, name, MAX_NAME - 1);
  t->arity = arity;
  for (int i = 0; i < arity; i++) {
    assert(args != NULL && "Args array is NULL but arity > 0");
    t->args[i] = args[i];
  }
  return t;
}

term_t *make_const(prolog_ctx_t *ctx, const char *name) {
  assert(name != NULL && "Constant name cannot be NULL");
  return make_term(ctx, CONST, name, NULL, 0);
}

term_t *make_var(prolog_ctx_t *ctx, const char *name) {
  assert(name != NULL && "Variable name cannot be NULL");
  return make_term(ctx, VAR, name, NULL, 0);
}

term_t *make_func(prolog_ctx_t *ctx, const char *name, term_t **args,
                  int arity) {
  assert(name != NULL && "Functor name cannot be NULL");
  assert(arity >= 0 && "Functor arity cannot be negative");
  return make_term(ctx, FUNC, name, args, arity);
}

term_t *rename_vars(prolog_ctx_t *ctx, term_t *t, int id) {
  if (!t)
    return NULL;

  if (t->type == CONST)
    return make_const(ctx, t->name);

  if (t->type == VAR) {
    char new_name[MAX_NAME];
    int written = snprintf(new_name, MAX_NAME, "%s#%d", t->name,
                           id); // use # instead of _
    assert(written > 0 && written < MAX_NAME &&
           "Variable name too long after renaming");
    return make_var(ctx, new_name);
  }

  assert(t->type == FUNC && "Invalid term type in rename_vars");

  term_t *args[MAX_ARGS];
  for (int i = 0; i < t->arity; i++) {
    args[i] = rename_vars(ctx, t->args[i], id);
  }
  return make_func(ctx, t->name, args, t->arity);
}