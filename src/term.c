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

term_t *make_var(prolog_ctx_t *ctx, const char *name, int var_id) {
  term_t *t = ctx_alloc_term(ctx);
  t->type = VAR;
  t->name = name ? intern_name(ctx, name) : NULL;
  t->arity = var_id; // var_id stored in arity field for VAR terms
  return t;
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

term_t *rename_vars_mapped(prolog_ctx_t *ctx, term_t *t, var_id_map_t *map) {
  if (!t)
    return NULL;
  if (t->type == CONST || t->type == STRING)
    return t;
  if (t->type == VAR) {
    int old_id = t->arity;
    for (int i = 0; i < map->count; i++) {
      if (map->entries[i].old_id == old_id)
        return make_var(ctx, NULL, map->entries[i].new_id);
    }
    int new_id = ctx->var_counter++;
    assert(map->count < MAX_CLAUSE_VARS && "Too many variables in clause");
    map->entries[map->count].old_id = old_id;
    map->entries[map->count].new_id = new_id;
    map->count++;
    return make_var(ctx, NULL, new_id);
  }
  assert(t->type == FUNC && "Invalid term type in rename_vars_mapped");
  term_t *args[MAX_ARGS];
  for (int i = 0; i < t->arity; i++)
    args[i] = rename_vars_mapped(ctx, t->args[i], map);
  return make_func(ctx, t->name, args, t->arity);
}

term_t *rename_vars(prolog_ctx_t *ctx, term_t *t) {
  var_id_map_t map = {0};
  return rename_vars_mapped(ctx, t, &map);
}