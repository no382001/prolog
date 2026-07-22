#include "platform_impl.h"

//****
//* allocation and interning
//****

void *term_alloc(trilog_ctx_t *ctx, size_t size) {
  size = (size + 7) & ~7; // 8-byte align
  if (ctx->alloc_permanent) {
    int new_perm = ctx->term_pool_perm - (int)size;
    if (new_perm < ctx->term_pool_offset) {
      ctx_runtime_error(ctx, "term pool exhausted (perm)");
      return NULL;
    }
    ctx->term_pool_perm = new_perm;
    void *ptr = ctx->term_pool + new_perm;
    memset(ptr, 0, size);
    return ptr;
  }
  if (ctx->term_pool_offset + (int)size > ctx->term_pool_perm) {
    ctx_runtime_error(ctx, "term pool exhausted");
    return NULL;
  }
  void *ptr = ctx->term_pool + ctx->term_pool_offset;
  memset(ptr, 0, size);
  ctx->term_pool_offset += (int)size;
  if (ctx->term_pool_offset > ctx->term_pool_peak)
    ctx->term_pool_peak = ctx->term_pool_offset;
  return ptr;
}

void ctx_reset_terms(trilog_ctx_t *ctx) {
  ctx->term_pool_offset = 0;
  ctx->term_pool_perm = ctx->term_pool_size;
  ctx->string_pool_offset = 0;
}

const char *intern_name(trilog_ctx_t *ctx, const char *name) {
  assert(ctx != NULL && "Context is NULL");
  assert(name != NULL && "Name is NULL");

  // fast path: name is already a canonical pointer into this pool (e.g. a
  // clause functor name copied during renaming) — no need to rescan.
  if (name >= ctx->string_pool &&
      name < ctx->string_pool + ctx->string_pool_offset)
    return name;

  int len = strlen(name);

  // search for existing copy in string pool
  int i = 0;
  while (i < ctx->string_pool_offset) {
    if (strcmp(&ctx->string_pool[i], name) == 0)
      return &ctx->string_pool[i];
    i += strlen(&ctx->string_pool[i]) + 1;
  }

  if (ctx->string_pool_offset + len + 1 > MAX_STRING_POOL) {
    ctx_runtime_error(ctx, "string pool exhausted");
    return name;
  }

  char *dest = &ctx->string_pool[ctx->string_pool_offset];
  if (dest != name)
    memcpy(dest, name, len + 1);
  ctx->string_pool_offset += len + 1;
  return dest;
}

//****
//* term constructors
//****

term_t *make_const(trilog_ctx_t *ctx, const char *name) {
  assert(name != NULL && "Constant name cannot be NULL");
  term_t *t = term_alloc(ctx, sizeof(term_t)); // no args
  if (!t)
    return NULL;
  t->type = CONST;
  t->name = intern_name(ctx, name);
  return t;
}

term_t *make_int(trilog_ctx_t *ctx, int n) {
  char buf[32];
  snprintf(buf, sizeof(buf), "%d", n);
  term_t *t = term_alloc(ctx, sizeof(term_t));
  if (!t)
    return NULL;
  t->type = INT;
  t->name = intern_name(ctx, buf);
  return t;
}

// canonical float formatting: try increasing precision until the string
// round-trips to the exact same double, so equal doubles always intern to
// the same string (unify.c compares FLOATs by interned-string equality).
// ISO requires a decimal point in the printed form, so "7" becomes "7.0".
term_t *make_float(trilog_ctx_t *ctx, double d) {
  char buf[64];
  for (int prec = 15; prec <= 17; prec++) {
    snprintf(buf, sizeof(buf), "%.*g", prec, d);
    if (strtod(buf, NULL) == d)
      break;
  }
  if (!strchr(buf, '.') && !strchr(buf, 'e') && !strchr(buf, 'E'))
    strcat(buf, ".0");
  term_t *t = term_alloc(ctx, sizeof(term_t));
  if (!t)
    return NULL;
  t->type = FLOAT;
  t->name = intern_name(ctx, buf);
  return t;
}

bool term_as_float(const term_t *t, double *out) {
  if (!t || t->type != FLOAT)
    return false;
  *out = strtod(t->name, NULL);
  return true;
}

term_t *make_var(trilog_ctx_t *ctx, const char *name, int var_id) {
  assert(var_id < MAX_VARS && "Variable table full");
  term_t *t = term_alloc(ctx, sizeof(term_t)); // no args
  if (!t)
    return NULL;
  t->type = VAR;
  if (name)
    t->name = intern_name(ctx, name);
  t->arity = var_id; // var_id stored in arity field for var terms
  return t;
}

term_t *make_func(trilog_ctx_t *ctx, const char *name, term_t **args,
                  int arity) {
  assert(name != NULL && "Functor name cannot be NULL");
  assert(arity >= 0 && "Functor arity cannot be negative");
  term_t *t = term_alloc(ctx, sizeof(term_t) + arity * sizeof(term_t *));
  if (!t)
    return NULL;
  t->type = FUNC;
  t->name = intern_name(ctx, name);
  t->arity = arity;
  for (int i = 0; i < arity; i++)
    t->args[i] = args[i];
  return t;
}

term_t *make_str(trilog_ctx_t *ctx, const char *data, int len) {
  term_t *t = term_alloc(ctx, sizeof(term_t));
  if (!t)
    return NULL;
  t->type = STR;
  t->name = data;
  t->arity = len;
  return t;
}

term_t *list_head(trilog_ctx_t *ctx, const term_t *t) {
  if (t->type == STR) {
    assert(t->arity > 0);
    char ch[2] = {t->name[0], '\0'};
    return make_const(ctx, ch);
  }
  return t->args[0];
}

term_t *list_tail(trilog_ctx_t *ctx, const term_t *t) {
  if (t->type == STR) {
    assert(t->arity > 0);
    return make_str(ctx, t->name + 1, t->arity - 1);
  }
  return t->args[1];
}

// make_term: backward compat wrapper (used in a few places in builtins)
term_t *make_term(trilog_ctx_t *ctx, term_type type, const char *name,
                  term_t **args, int arity) {
  assert(ctx != NULL && "Context is NULL");
  assert(name != NULL && "Term name cannot be NULL");
  assert(arity >= 0 && arity <= MAX_ARGS && "Invalid arity");
  assert((type == CONST || type == VAR || type == FUNC || type == INT) &&
         "Invalid term type");

  if (type == CONST)
    return make_const(ctx, name);
  if (type == VAR)
    return make_var(ctx, name, arity);
  return make_func(ctx, name, args, arity);
}

//****
//* variable renaming
//****

term_t *rename_vars_mapped_named(trilog_ctx_t *ctx, term_t *t,
                                 var_id_map_t *map, bool preserve_names) {
  if (!t)
    return NULL;
  if (t->type == CONST || t->type == INT || t->type == STR || t->type == FLOAT)
    return t;
  if (t->type == VAR) {
    const char *name = preserve_names ? t->name : NULL;
    int old_id = t->arity;
    for (int i = 0; i < map->count; i++) {
      if (map->entries[i].old_id == old_id)
        return make_var(ctx, name, map->entries[i].new_id);
    }
    int new_id = ctx->var_counter++;
    assert(map->count < MAX_CLAUSE_VARS && "Too many variables in clause");
    map->entries[map->count].old_id = old_id;
    map->entries[map->count].new_id = new_id;
    map->count++;
    return make_var(ctx, name, new_id);
  }
  assert(t->type == FUNC && "Invalid term type in rename_vars_mapped");
  term_t *args[MAX_ARGS];
  for (int i = 0; i < t->arity; i++)
    args[i] = rename_vars_mapped_named(ctx, t->args[i], map, preserve_names);
  return make_func(ctx, t->name, args, t->arity);
}

term_t *rename_vars_mapped(trilog_ctx_t *ctx, term_t *t, var_id_map_t *map) {
  return rename_vars_mapped_named(ctx, t, map, false);
}

term_t *rename_vars(trilog_ctx_t *ctx, term_t *t) {
  var_id_map_t map = {0};
  return rename_vars_mapped(ctx, t, &map);
}

//****
//* perm pool compaction
//****

static term_t *copy_term_into_pool(trilog_ctx_t *ctx, term_t *t) {
  if (!t)
    return NULL;
  switch (t->type) {
  case CONST:
    return make_const(ctx, t->name);
  case INT: {
    int v = 0;
    term_as_int(t, &v);
    return make_int(ctx, v);
  }
  case FLOAT: {
    double v = 0;
    term_as_float(t, &v);
    return make_float(ctx, v);
  }
  case VAR:
    return make_var(ctx, t->name, t->arity);
  case STR:
    return make_str(ctx, t->name, t->arity);
  case FUNC: {
    term_t *args[MAX_ARGS];
    for (int i = 0; i < t->arity; i++) {
      args[i] = copy_term_into_pool(ctx, t->args[i]);
      if (!args[i])
        return NULL;
    }
    return make_func(ctx, t->name, args, t->arity);
  }
  }
  return NULL;
}

// find the lowest perm-pool offset reachable from a term tree
static int min_perm_offset(term_t *t, char *base, int perm_lo, int perm_hi) {
  if (!t)
    return perm_hi;
  int off = (int)((char *)t - base);
  if (off < perm_lo || off >= perm_hi)
    return perm_hi; // not in perm region
  int mn = off;
  if (t->type == FUNC)
    for (int i = 0; i < t->arity; i++) {
      int c = min_perm_offset(t->args[i], base, perm_lo, perm_hi);
      if (c < mn)
        mn = c;
    }
  return mn;
}

// trim dead space at the bottom of the perm pool by finding the lowest
// address still referenced by a live clause and raising term_pool_perm.
// zero-copy, works regardless of pool fullness.
static void trim_perm_pool(trilog_ctx_t *ctx) {
  int perm_lo = ctx->term_pool_perm;
  int perm_hi = ctx->term_pool_size;
  if (perm_lo >= perm_hi)
    return;
  if (ctx->db_count == 0) {
    ctx->term_pool_perm = ctx->term_pool_size;
    return;
  }
  char *base = ctx->term_pool;
  int mn = perm_hi;
  for (int i = 0; i < ctx->db_count; i++) {
    clause_t *c = &ctx->database[i];
    int h = min_perm_offset(c->head, base, perm_lo, perm_hi);
    if (h < mn)
      mn = h;
    if (c->body) {
      int boff = (int)((char *)c->body - base);
      if (boff >= perm_lo && boff < mn)
        mn = boff;
      for (int j = 0; j < c->body_count; j++) {
        int g = min_perm_offset(c->body[j], base, perm_lo, perm_hi);
        if (g < mn)
          mn = g;
      }
    }
  }
  if (mn > perm_lo)
    ctx->term_pool_perm = mn;
}

static void rebase_term_ptrs(term_t *t, char *old_base, char *new_base) {
  if (!t || t->type != FUNC)
    return;
  for (int i = 0; i < t->arity; i++) {
    if (t->args[i]) {
      t->args[i] = (term_t *)(new_base + ((char *)t->args[i] - old_base));
      rebase_term_ptrs(t->args[i], old_base, new_base);
    }
  }
}

void compact_perm_pool(trilog_ctx_t *ctx) {
  if (ctx->term_pool_offset != 0)
    return;

  // phase 1: trim contiguous dead space at the bottom of perm (zero-copy)
  trim_perm_pool(ctx);

  // phase 2: full defrag for interleaved dead space (needs perm < 50%)
  int perm_start = ctx->term_pool_perm;
  int perm_used = ctx->term_pool_size - perm_start;
  if (perm_used <= 0 || perm_used * 2 > ctx->term_pool_size)
    return;

  char *old_base = ctx->term_pool + perm_start;
  memcpy(ctx->term_pool, old_base, (size_t)perm_used);
  char *staging = ctx->term_pool;
  ctx->term_pool_offset = perm_used;

  for (int i = 0; i < ctx->db_count; i++) {
    clause_t *c = &ctx->database[i];
    if (c->body != NULL) {
      term_t **sb = (term_t **)(staging + ((char *)c->body - old_base));
      for (int j = 0; j < c->body_count; j++)
        if (sb[j])
          sb[j] = (term_t *)(staging + ((char *)sb[j] - old_base));
      c->body = sb;
    }
    if (c->head)
      c->head = (term_t *)(staging + ((char *)c->head - old_base));
  }

  for (int i = 0; i < ctx->db_count; i++) {
    clause_t *c = &ctx->database[i];
    if (c->head)
      rebase_term_ptrs(c->head, old_base, staging);
    if (c->body != NULL)
      for (int j = 0; j < c->body_count; j++)
        if (c->body[j])
          rebase_term_ptrs(c->body[j], old_base, staging);
  }

  ctx->term_pool_perm = ctx->term_pool_size;
  ctx->alloc_permanent = true;
  for (int i = 0; i < ctx->db_count; i++) {
    clause_t *c = &ctx->database[i];
    if (c->head) {
      c->head = copy_term_into_pool(ctx, c->head);
      if (!c->head) {
        ctx->alloc_permanent = false;
        ctx->term_pool_offset = 0;
        return;
      }
    }
    if (c->body_count > 0) {
      term_t **nb =
          (term_t **)term_alloc(ctx, (size_t)c->body_count * sizeof(term_t *));
      if (!nb) {
        ctx->alloc_permanent = false;
        ctx->term_pool_offset = 0;
        return;
      }
      for (int j = 0; j < c->body_count; j++) {
        nb[j] = c->body[j] ? copy_term_into_pool(ctx, c->body[j]) : NULL;
        if (c->body[j] && !nb[j]) {
          ctx->alloc_permanent = false;
          ctx->term_pool_offset = 0;
          return;
        }
      }
      c->body = nb;
    } else {
      c->body = NULL;
    }
  }
  ctx->alloc_permanent = false;
  ctx->term_pool_offset = 0;
}
