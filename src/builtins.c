#include "platform_impl.h"

typedef struct {
  const char *op;
  int (*fn)(int, int);
} arith_op_t;

static int arith_add(int a, int b) { return a + b; }
static int arith_sub(int a, int b) { return a - b; }
static int arith_mul(int a, int b) { return a * b; }
static int arith_div(int a, int b) { return b ? a / b : 0; }
static int arith_mod(int a, int b) { return b ? a % b : 0; }

static const arith_op_t arith_ops[] = {{"+", arith_add},   {"-", arith_sub},
                                       {"*", arith_mul},   {"/", arith_div},
                                       {"mod", arith_mod}, {NULL, NULL}};

static bool eval_arith(prolog_ctx_t *ctx, term_t *t, env_t *env, int *result) {
  t = deref(env, t);

  if (t->type == CONST) {
    char *end;
    long val = strtol(t->name, &end, 10);
    if (*end == '\0') {
      *result = (int)val;
      return true;
    }
    return false;
  }

  if (t->type == FUNC && t->arity == 2) {
    int left, right;
    if (!eval_arith(ctx, t->args[0], env, &left))
      return false;
    if (!eval_arith(ctx, t->args[1], env, &right))
      return false;

    for (const arith_op_t *op = arith_ops; op->op; op++) {
      if (strcmp(t->name, op->op) == 0) {
        *result = op->fn(left, right);
        return true;
      }
    }
  }

  return false;
}

typedef struct {
  const char *name;
  int arity; // -1 means any arity, 0 for CONST
  int (*handler)(prolog_ctx_t *ctx, term_t *goal, env_t *env);
} builtin_t;

static int builtin_true(prolog_ctx_t *ctx, term_t *goal, env_t *env) {
  (void)ctx;
  (void)goal;
  (void)env;
  return 1;
}

static int builtin_fail(prolog_ctx_t *ctx, term_t *goal, env_t *env) {
  (void)ctx;
  (void)goal;
  (void)env;
  return -1;
}

static int builtin_is(prolog_ctx_t *ctx, term_t *goal, env_t *env) {
  int result;
  if (!eval_arith(ctx, goal->args[1], env, &result))
    return -1;
  char buf[32];
  snprintf(buf, sizeof(buf), "%d", result);
  term_t *result_term = make_const(ctx, buf);
  return unify(ctx, goal->args[0], result_term, env) ? 1 : -1;
}

static int builtin_unify(prolog_ctx_t *ctx, term_t *goal, env_t *env) {
  return unify(ctx, goal->args[0], goal->args[1], env) ? 1 : -1;
}

static int builtin_not_unify(prolog_ctx_t *ctx, term_t *goal, env_t *env) {
  int old_count = env->count;
  bool unified = unify(ctx, goal->args[0], goal->args[1], env);
  env->count = old_count;
  return unified ? -1 : 1;
}

static int builtin_lt(prolog_ctx_t *ctx, term_t *goal, env_t *env) {
  int left, right;
  if (!eval_arith(ctx, goal->args[0], env, &left))
    return 0;
  if (!eval_arith(ctx, goal->args[1], env, &right))
    return 0;
  return left < right ? 1 : -1;
}

static int builtin_gt(prolog_ctx_t *ctx, term_t *goal, env_t *env) {
  int left, right;
  if (!eval_arith(ctx, goal->args[0], env, &left))
    return 0;
  if (!eval_arith(ctx, goal->args[1], env, &right))
    return 0;
  return left > right ? 1 : -1;
}

static int builtin_le(prolog_ctx_t *ctx, term_t *goal, env_t *env) {
  int left, right;
  if (!eval_arith(ctx, goal->args[0], env, &left))
    return 0;
  if (!eval_arith(ctx, goal->args[1], env, &right))
    return 0;
  return left <= right ? 1 : -1;
}

static int builtin_ge(prolog_ctx_t *ctx, term_t *goal, env_t *env) {
  int left, right;
  if (!eval_arith(ctx, goal->args[0], env, &left))
    return 0;
  if (!eval_arith(ctx, goal->args[1], env, &right))
    return 0;
  return left >= right ? 1 : -1;
}

static int builtin_arith_eq(prolog_ctx_t *ctx, term_t *goal, env_t *env) {
  int left, right;
  if (!eval_arith(ctx, goal->args[0], env, &left))
    return 0;
  if (!eval_arith(ctx, goal->args[1], env, &right))
    return 0;
  return left == right ? 1 : -1;
}

static int builtin_arith_ne(prolog_ctx_t *ctx, term_t *goal, env_t *env) {
  int left, right;
  if (!eval_arith(ctx, goal->args[0], env, &left))
    return 0;
  if (!eval_arith(ctx, goal->args[1], env, &right))
    return 0;
  return left != right ? 1 : -1;
}

static int builtin_cut(prolog_ctx_t *ctx, term_t *goal, env_t *env) {
  (void)ctx;
  (void)goal;
  (void)env;
  return 2;
}

static int builtin_stats(prolog_ctx_t *ctx, term_t *goal, env_t *env) {
  io_writef(ctx, "terms: %d allocated, %d peak, %d current\n",
            ctx->stats.terms_allocated, ctx->stats.terms_peak, ctx->term_count);
  io_writef(ctx, "unify: %d calls, %d fails\n", ctx->stats.unify_calls,
            ctx->stats.unify_fails);
  io_writef(ctx, "solve: %d son calls, %d backtracks\n", ctx->stats.son_calls,
            ctx->stats.backtracks);
  return 1;

  return 2;
}

typedef struct {
  prolog_ctx_t *ctx;
  term_t *template;
  term_t **results;
  int count;
  int max;
} findall_state_t;

static bool findall_callback(prolog_ctx_t *ctx, env_t *env, void *userdata) {
  findall_state_t *state = (findall_state_t *)userdata;

  if (state->count < state->max) {
    state->results[state->count++] = substitute(ctx, env, state->template);
  }

  return true; // continue finding more
}

static term_t *build_list(prolog_ctx_t *ctx, term_t **items, int count) {
  term_t *list = make_const(ctx, "[]");
  for (int i = count - 1; i >= 0; i--) {
    term_t *args[2] = {items[i], list};
    list = make_func(ctx, ".", args, 2);
  }
  return list;
}

static int builtin_findall(prolog_ctx_t *ctx, term_t *goal, env_t *env) {
  term_t *template = deref(env, goal->args[0]);
  term_t *query = deref(env, goal->args[1]);
  term_t *result_list = goal->args[2];

  // rename variables to avoid conflicts
  int id = ++ctx->var_counter;
  template = rename_vars(ctx, template, id);
  query = rename_vars(ctx, query, id);

  // build goal statement
  goal_stmt_t goals = {0};
  goals.goals[goals.count++] = query;

  // collect solutions
  term_t *results[MAX_TERMS];
  findall_state_t state = {.ctx = ctx,
                           .template = template,
                           .results = results,
                           .count = 0,
                           .max = MAX_TERMS};

  env_t query_env = {0};
  solve_all(ctx, &goals, &query_env, findall_callback, &state);

  term_t *list = build_list(ctx, results, state.count);
  return unify(ctx, result_list, list, env) ? 1 : -1;
}

static int builtin_bagof(prolog_ctx_t *ctx, term_t *goal, env_t *env) {
  term_t *template = deref(env, goal->args[0]);
  term_t *query = deref(env, goal->args[1]);
  term_t *result_list = goal->args[2];

  int id = ++ctx->var_counter;
  template = rename_vars(ctx, template, id);
  query = rename_vars(ctx, query, id);

  goal_stmt_t goals = {0};
  goals.goals[goals.count++] = query;

  term_t *results[MAX_TERMS];
  findall_state_t state = {.ctx = ctx,
                           .template = template,
                           .results = results,
                           .count = 0,
                           .max = MAX_TERMS};

  env_t query_env = {0};
  solve_all(ctx, &goals, &query_env, findall_callback, &state);

  // bagof fails if no solutions
  if (state.count == 0)
    return -1;

  term_t *list = build_list(ctx, results, state.count);
  return unify(ctx, result_list, list, env) ? 1 : -1;
}

static int builtin_include(prolog_ctx_t *ctx, term_t *goal, env_t *env) {
  term_t *arg = deref(env, goal->args[0]);
  const char *filename = NULL;
  if (arg->type == STRING)
    filename = arg->string_data;
  else if (arg->type == CONST)
    filename = arg->name;
  else
    return -1;

  // resolve relative paths against the directory of the file being loaded
  char resolved[MAX_FILE_PATH];
  if (filename[0] != '/' && ctx->load_dir[0] != '\0') {
    snprintf(resolved, sizeof(resolved), "%s/%s", ctx->load_dir, filename);
    filename = resolved;
  }

  return prolog_load_file(ctx, filename) ? 1 : -1;
}

static const builtin_t builtins[] = {
    // 0-arity
    {"true", 0, builtin_true},
    {"fail", 0, builtin_fail},
    {"!", 0, builtin_cut},
    {"stats", 0, builtin_stats},
    // 2-arity
    {"is", 2, builtin_is},
    {"=", 2, builtin_unify},
    {"\\=", 2, builtin_not_unify},
    {"<", 2, builtin_lt},
    {">", 2, builtin_gt},
    {"=<", 2, builtin_le},
    {">=", 2, builtin_ge},
    {"=:=", 2, builtin_arith_eq},
    {"=\\=", 2, builtin_arith_ne},
    // 1-arity
    {"include", 1, builtin_include},
    // 3-arity
    {"findall", 3, builtin_findall},
    {"bagof", 3, builtin_bagof},
    {NULL, 0, NULL}};

int try_builtin(prolog_ctx_t *ctx, term_t *goal, env_t *env) {
  goal = deref(env, goal);

  const char *name = goal->name;
  int arity = (goal->type == CONST)  ? 0
              : (goal->type == FUNC) ? goal->arity
                                     : -1;

  if (arity < 0)
    return 0;

  for (int i = 0; i < ctx->custom_builtin_count; i++) {
    custom_builtin_t *cb = &ctx->custom_builtins[i];
    if (strcmp(name, cb->name) == 0 &&
        (cb->arity == -1 || arity == cb->arity)) {
      return cb->handler(ctx, goal, env);
    }
  }

  for (const builtin_t *b = builtins; b->name; b++) {
    if (strcmp(name, b->name) == 0 && arity == b->arity) {
      return b->handler(ctx, goal, env);
    }
  }

  return 0;
}