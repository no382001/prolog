#include "platform_impl.h"

void ctx_reset_terms(prolog_ctx_t *ctx) {
  ctx->term_count = 0;
  ctx->string_pool_offset = 0;
}

const char *intern_name(prolog_ctx_t *ctx, const char *name) {
  assert(ctx != NULL && "Context is NULL");
  assert(name != NULL && "Name is NULL");

  int len = strlen(name);

  // search for existing copy in string pool
  int i = 0;
  while (i < ctx->string_pool_offset) {
    if (strcmp(&ctx->string_pool[i], name) == 0)
      return &ctx->string_pool[i];
    i += strlen(&ctx->string_pool[i]) + 1;
  }

  assert(ctx->string_pool_offset + len + 1 <= MAX_STRING_POOL &&
         "String pool exhausted");

  char *dest = &ctx->string_pool[ctx->string_pool_offset];
  memcpy(dest, name, len + 1);
  ctx->string_pool_offset += len + 1;
  return dest;
}

term_t *ctx_alloc_term(prolog_ctx_t *ctx) {
  assert(ctx->term_count < MAX_TERMS && "Term pool exhausted");

  ctx->stats.terms_allocated++;
  if (ctx->term_count > ctx->stats.terms_peak)
    ctx->stats.terms_peak = ctx->term_count;

  term_t *t = &ctx->term_pool[ctx->term_count++];
  memset(t, 0, sizeof(term_t));
  return t;
}

term_t *make_term(prolog_ctx_t *ctx, term_type type, const char *name,
                  term_t **args, int arity) {
  assert(ctx != NULL && "Context is NULL");
  assert(name != NULL && "Term name cannot be NULL");
  assert(arity >= 0 && arity <= MAX_ARGS && "Invalid arity");
  assert((type == CONST || type == VAR || type == FUNC || type == STRING) &&
         "Invalid term type");

  term_t *t = ctx_alloc_term(ctx);

  t->type = type;
  t->name = intern_name(ctx, name);
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

term_t *make_string(prolog_ctx_t *ctx, const char *str) {
  assert(ctx != NULL && "Context is NULL");
  assert(str != NULL && "String cannot be NULL");

  term_t *t = ctx_alloc_term(ctx);
  t->type = STRING;
  t->name = "";
  t->arity = 0;

  int len = strlen(str);
  assert(ctx->string_pool_offset + len + 1 <= MAX_STRING_POOL &&
         "String pool exhausted");

  t->string_data = &ctx->string_pool[ctx->string_pool_offset];
  strcpy(t->string_data, str);
  ctx->string_pool_offset += len + 1;

  return t;
}

term_t *rename_vars(prolog_ctx_t *ctx, term_t *t, int id) {
  if (!t)
    return NULL;

  if (t->type == CONST)
    return t;

  if (t->type == STRING)
    return t;

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