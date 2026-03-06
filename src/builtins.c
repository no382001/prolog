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

  if (term_as_int(t, result))
    return true;

  if (t->type == FUNC && t->arity == 1) {
    int val;
    if (!eval_arith(ctx, t->args[0], env, &val))
      return false;
    if (strcmp(t->name, "-") == 0) {
      *result = -val;
      return true;
    }
    if (strcmp(t->name, "abs") == 0) {
      *result = val < 0 ? -val : val;
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

    if (strcmp(t->name, "max") == 0) {
      *result = left > right ? left : right;
      return true;
    }
    if (strcmp(t->name, "min") == 0) {
      *result = left < right ? left : right;
      return true;
    }
    if (strcmp(t->name, "//") == 0) {
      *result = right ? left / right : 0;
      return true;
    }

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
  builtin_result_t (*handler)(prolog_ctx_t *ctx, term_t *goal, env_t *env);
} builtin_t;

static builtin_result_t builtin_true(prolog_ctx_t *ctx, term_t *goal,
                                     env_t *env) {
  (void)ctx;
  (void)goal;
  (void)env;
  return BUILTIN_OK;
}

static builtin_result_t builtin_fail(prolog_ctx_t *ctx, term_t *goal,
                                     env_t *env) {
  (void)ctx;
  (void)goal;
  (void)env;
  return BUILTIN_FAIL;
}

static builtin_result_t builtin_is(prolog_ctx_t *ctx, term_t *goal,
                                   env_t *env) {
  int result;
  if (!eval_arith(ctx, goal->args[1], env, &result))
    return BUILTIN_FAIL;
  char buf[32];
  snprintf(buf, sizeof(buf), "%d", result);
  term_t *result_term = make_const(ctx, buf);
  return unify(ctx, goal->args[0], result_term, env) ? BUILTIN_OK
                                                     : BUILTIN_FAIL;
}

static builtin_result_t builtin_unify(prolog_ctx_t *ctx, term_t *goal,
                                      env_t *env) {
  return unify(ctx, goal->args[0], goal->args[1], env) ? BUILTIN_OK
                                                       : BUILTIN_FAIL;
}

static builtin_result_t builtin_not_unify(prolog_ctx_t *ctx, term_t *goal,
                                          env_t *env) {
  int old_count = env->count;
  bool unified = unify(ctx, goal->args[0], goal->args[1], env);
  env->count = old_count;
  return unified ? -1 : 1;
}

static builtin_result_t builtin_lt(prolog_ctx_t *ctx, term_t *goal,
                                   env_t *env) {
  int left, right;
  if (!eval_arith(ctx, goal->args[0], env, &left))
    return BUILTIN_NOT_HANDLED;
  if (!eval_arith(ctx, goal->args[1], env, &right))
    return BUILTIN_NOT_HANDLED;
  return left < right ? BUILTIN_OK : BUILTIN_FAIL;
}

#define ARITH_CMP_BUILTIN(name, op)                                            \
  static builtin_result_t name(prolog_ctx_t *ctx, term_t *goal, env_t *env) {  \
    int left, right;                                                           \
    if (!eval_arith(ctx, goal->args[0], env, &left))                           \
      return BUILTIN_NOT_HANDLED;                                              \
    if (!eval_arith(ctx, goal->args[1], env, &right))                          \
      return BUILTIN_NOT_HANDLED;                                              \
    return left op right ? BUILTIN_OK : BUILTIN_FAIL;                          \
  }

ARITH_CMP_BUILTIN(builtin_gt, >)
ARITH_CMP_BUILTIN(builtin_le, <=)
ARITH_CMP_BUILTIN(builtin_ge, >=)
ARITH_CMP_BUILTIN(builtin_arith_eq, ==)
ARITH_CMP_BUILTIN(builtin_arith_ne, !=)

#undef ARITH_CMP_BUILTIN

static builtin_result_t builtin_cut(prolog_ctx_t *ctx, term_t *goal,
                                    env_t *env) {
  (void)ctx;
  (void)goal;
  (void)env;
  return BUILTIN_CUT;
}

static builtin_result_t builtin_stats(prolog_ctx_t *ctx, term_t *goal,
                                      env_t *env) {
  (void)goal;
  (void)env;
  io_writef(ctx, "terms: %d allocated, %d peak, %d current\n",
            ctx->stats.terms_allocated, ctx->stats.terms_peak, ctx->term_count);
  io_writef(ctx, "unify: %d calls, %d fails\n", ctx->stats.unify_calls,
            ctx->stats.unify_fails);
  io_writef(ctx, "solve: %d son calls, %d backtracks\n", ctx->stats.son_calls,
            ctx->stats.backtracks);
  return BUILTIN_OK;
}

typedef struct {
  prolog_ctx_t *ctx;
  term_t *template;
  term_t *list;
  int count;
} findall_state_t;

static bool findall_callback(prolog_ctx_t *ctx, env_t *env, void *userdata,
                             bool has_more) {
  (void)has_more;
  findall_state_t *state = userdata;
  term_t *val = substitute(ctx, env, state->template);
  term_t *args[2] = {val, state->list};
  state->list = make_func(ctx, ".", args, 2);
  state->count++;
  return true;
}

static term_t *reverse_list(prolog_ctx_t *ctx, term_t *list) {
  term_t *result = make_const(ctx, "[]");
  while (is_cons(list)) {
    term_t *args[2] = {list->args[0], result};
    result = make_func(ctx, ".", args, 2);
    list = list->args[1];
  }
  return result;
}

static int collect_solutions(prolog_ctx_t *ctx, term_t *goal, env_t *env,
                             bool fail_on_empty) {
  term_t *template = deref(env, goal->args[0]);
  term_t *query = deref(env, goal->args[1]);
  term_t *result_var = goal->args[2];

  var_id_map_t _map = {0};
  template = rename_vars_mapped(ctx, template, &_map);
  query = rename_vars_mapped(ctx, query, &_map);

  goal_stmt_t goals = {0};
  goals.goals[goals.count++] = query;

  findall_state_t state = {.ctx = ctx,
                           .template = template,
                           .list = make_const(ctx, "[]"),
                           .count = 0};

  env_t query_env = {0};
  solve_all(ctx, &goals, &query_env, findall_callback, &state);

  if (fail_on_empty && state.count == 0)
    return BUILTIN_FAIL;

  term_t *result = reverse_list(ctx, state.list);
  return unify(ctx, result_var, result, env) ? BUILTIN_OK : BUILTIN_FAIL;
}

static builtin_result_t builtin_findall(prolog_ctx_t *ctx, term_t *goal,
                                        env_t *env) {
  return collect_solutions(ctx, goal, env, false);
}

static builtin_result_t builtin_bagof(prolog_ctx_t *ctx, term_t *goal,
                                      env_t *env) {
  return collect_solutions(ctx, goal, env, true);
}

static bool not_found_callback(prolog_ctx_t *ctx, env_t *env, void *userdata,
                               bool has_more) {
  (void)ctx;
  (void)env;
  (void)has_more;
  *(bool *)userdata = true;
  return false;
}

static builtin_result_t builtin_once(prolog_ctx_t *ctx, term_t *goal,
                                     env_t *env) {
  term_t *inner = deref(env, goal->args[0]);
  goal_stmt_t goals = {0};
  goals.goals[goals.count++] = inner;
  return solve(ctx, &goals, env) ? BUILTIN_OK : BUILTIN_FAIL;
}

static builtin_result_t builtin_not(prolog_ctx_t *ctx, term_t *goal,
                                    env_t *env) {
  term_t *inner = deref(env, goal->args[0]);
  goal_stmt_t goals = {0};
  goals.goals[goals.count++] = inner;
  int env_mark = env->count;
  bool found = false;
  solve_all(ctx, &goals, env, not_found_callback, &found);
  env->count = env_mark;
  return found ? -1 : 1;
}

static bool is_integer_str(const char *s) {
  if (*s == '-')
    s++;
  if (!*s)
    return false;
  while (*s)
    if (!isdigit((unsigned char)*s++))
      return false;
  return true;
}

static builtin_result_t builtin_var(prolog_ctx_t *ctx, term_t *goal,
                                    env_t *env) {
  (void)ctx;
  return deref(env, goal->args[0])->type == VAR ? BUILTIN_OK : BUILTIN_FAIL;
}

static builtin_result_t builtin_nonvar(prolog_ctx_t *ctx, term_t *goal,
                                       env_t *env) {
  (void)ctx;
  return deref(env, goal->args[0])->type != VAR ? BUILTIN_OK : BUILTIN_FAIL;
}

static builtin_result_t builtin_atom(prolog_ctx_t *ctx, term_t *goal,
                                     env_t *env) {
  (void)ctx;
  term_t *t = deref(env, goal->args[0]);
  return (t->type == CONST && !is_integer_str(t->name)) ? BUILTIN_OK
                                                        : BUILTIN_FAIL;
}

static builtin_result_t builtin_integer(prolog_ctx_t *ctx, term_t *goal,
                                        env_t *env) {
  (void)ctx;
  term_t *t = deref(env, goal->args[0]);
  return (t->type == CONST && is_integer_str(t->name)) ? BUILTIN_OK
                                                       : BUILTIN_FAIL;
}

static builtin_result_t builtin_is_list(prolog_ctx_t *ctx, term_t *goal,
                                        env_t *env) {
  (void)ctx;
  term_t *t = deref(env, goal->args[0]);
  while (is_cons(t))
    t = deref(env, t->args[1]);
  return is_nil(t) ? BUILTIN_OK : BUILTIN_FAIL;
}

static builtin_result_t builtin_nl(prolog_ctx_t *ctx, term_t *goal,
                                   env_t *env) {
  (void)goal;
  (void)env;
  io_write_str(ctx, "\n");
  return BUILTIN_OK;
}

static builtin_result_t builtin_write(prolog_ctx_t *ctx, term_t *goal,
                                      env_t *env) {
  io_write_term(ctx, deref(env, goal->args[0]), env);
  return BUILTIN_OK;
}

static builtin_result_t builtin_writeln(prolog_ctx_t *ctx, term_t *goal,
                                        env_t *env) {
  io_write_term(ctx, deref(env, goal->args[0]), env);
  io_write_str(ctx, "\n");
  return BUILTIN_OK;
}

static builtin_result_t builtin_writeq(prolog_ctx_t *ctx, term_t *goal,
                                       env_t *env) {
  io_write_term_quoted(ctx, deref(env, goal->args[0]), env);
  return BUILTIN_OK;
}

static const char *resolve_filename(prolog_ctx_t *ctx, term_t *arg, char *buf,
                                    size_t bufsz) {
  const char *filename = NULL;
  if (arg->type == STRING)
    filename = arg->string_data;
  else if (arg->type == CONST)
    filename = arg->name;
  else
    return NULL;

  if (filename[0] != '/' && ctx->load_dir[0] != '\0') {
    snprintf(buf, bufsz, "%s/%s", ctx->load_dir, filename);
    return buf;
  }
  return filename;
}

// consult/1: load file as independent unit, tracked for make/0
static builtin_result_t builtin_consult(prolog_ctx_t *ctx, term_t *goal,
                                        env_t *env) {
  char resolved[MAX_FILE_PATH];
  const char *filename = resolve_filename(ctx, deref(env, goal->args[0]),
                                          resolved, sizeof(resolved));
  if (!filename)
    return BUILTIN_FAIL;
  return prolog_load_file(ctx, filename) ? BUILTIN_OK : BUILTIN_FAIL;
}

// include/1: textual inclusion — only valid as a file directive,
// clauses are owned by the including file, not tracked for make/0
static builtin_result_t builtin_include(prolog_ctx_t *ctx, term_t *goal,
                                        env_t *env) {
  if (ctx->include_depth == 0) {
    io_writef(ctx, "Error: include/1 is only valid as a file directive\n");
    return BUILTIN_FAIL;
  }
  char resolved[MAX_FILE_PATH];
  const char *filename = resolve_filename(ctx, deref(env, goal->args[0]),
                                          resolved, sizeof(resolved));
  if (!filename)
    return BUILTIN_FAIL;
  // include_depth >= 1 here, so prolog_load_file won't track this file for
  // make/0
  return prolog_load_file(ctx, filename) ? BUILTIN_OK : BUILTIN_FAIL;
}

static const char *term_atom_str(const term_t *t) {
  if (!t)
    return NULL;
  if (t->type == STRING)
    return t->string_data;
  if (t->type == CONST)
    return t->name;
  return NULL;
}

static builtin_result_t builtin_compound(prolog_ctx_t *ctx, term_t *goal,
                                         env_t *env) {
  (void)ctx;
  term_t *t = deref(env, goal->args[0]);
  return (t->type == FUNC && t->arity > 0) ? BUILTIN_OK : BUILTIN_FAIL;
}
static builtin_result_t builtin_callable(prolog_ctx_t *ctx, term_t *goal,
                                         env_t *env) {
  (void)ctx;
  term_t *t = deref(env, goal->args[0]);
  return (t->type == CONST || t->type == FUNC) ? BUILTIN_OK : BUILTIN_FAIL;
}
static builtin_result_t builtin_number(prolog_ctx_t *ctx, term_t *goal,
                                       env_t *env) {
  (void)ctx;
  term_t *t = deref(env, goal->args[0]);
  return (t->type == CONST && is_integer_str(t->name)) ? BUILTIN_OK
                                                       : BUILTIN_FAIL;
}
static builtin_result_t builtin_atomic(prolog_ctx_t *ctx, term_t *goal,
                                       env_t *env) {
  (void)ctx;
  term_t *t = deref(env, goal->args[0]);
  return (t->type == CONST || t->type == STRING) ? BUILTIN_OK : BUILTIN_FAIL;
}
static builtin_result_t builtin_string(prolog_ctx_t *ctx, term_t *goal,
                                       env_t *env) {
  (void)ctx;
  term_t *t = deref(env, goal->args[0]);
  return (t->type == STRING) ? BUILTIN_OK : BUILTIN_FAIL;
}

static builtin_result_t builtin_atom_length(prolog_ctx_t *ctx, term_t *goal,
                                            env_t *env) {
  const char *s = term_atom_str(deref(env, goal->args[0]));
  if (!s)
    return BUILTIN_FAIL;
  char buf[16];
  snprintf(buf, sizeof(buf), "%d", (int)strlen(s));
  return unify(ctx, goal->args[1], make_const(ctx, buf), env) ? BUILTIN_OK
                                                              : BUILTIN_FAIL;
}

static builtin_result_t builtin_atom_concat(prolog_ctx_t *ctx, term_t *goal,
                                            env_t *env) {
  term_t *a = deref(env, goal->args[0]);
  term_t *b = deref(env, goal->args[1]);
  term_t *c = deref(env, goal->args[2]);
  const char *sa = term_atom_str(a);
  const char *sb = term_atom_str(b);
  const char *sc = term_atom_str(c);

  if (sa && sb) {
    char buf[MAX_NAME];
    int r = snprintf(buf, sizeof(buf), "%s%s", sa, sb);
    if (r < 0 || r >= (int)sizeof(buf))
      return BUILTIN_FAIL;
    return unify(ctx, goal->args[2], make_const(ctx, buf), env) ? BUILTIN_OK
                                                                : BUILTIN_FAIL;
  }
  if (sc && sa && b->type == VAR) {
    size_t la = strlen(sa), lc = strlen(sc);
    if (lc < la || strncmp(sc, sa, la) != 0)
      return BUILTIN_FAIL;
    return unify(ctx, goal->args[1], make_const(ctx, sc + la), env)
               ? BUILTIN_OK
               : BUILTIN_FAIL;
  }
  if (sc && sb && a->type == VAR) {
    size_t lb = strlen(sb), lc = strlen(sc);
    if (lc < lb || strcmp(sc + lc - lb, sb) != 0)
      return BUILTIN_FAIL;
    char buf[MAX_NAME];
    size_t la = lc - lb;
    if (la >= MAX_NAME)
      return BUILTIN_FAIL;
    strncpy(buf, sc, la);
    buf[la] = '\0';
    return unify(ctx, goal->args[0], make_const(ctx, buf), env) ? BUILTIN_OK
                                                                : BUILTIN_FAIL;
  }
  return BUILTIN_FAIL;
}

static builtin_result_t builtin_atom_chars(prolog_ctx_t *ctx, term_t *goal,
                                           env_t *env) {
  term_t *atom = deref(env, goal->args[0]);
  if (atom->type != VAR) {
    const char *s = term_atom_str(atom);
    if (!s)
      return BUILTIN_FAIL;
    term_t *list = make_const(ctx, "[]");
    for (int i = (int)strlen(s) - 1; i >= 0; i--) {
      char ch[2] = {s[i], '\0'};
      term_t *args[2] = {make_const(ctx, ch), list};
      list = make_func(ctx, ".", args, 2);
    }
    return unify(ctx, goal->args[1], list, env) ? BUILTIN_OK : BUILTIN_FAIL;
  }
  /* chars -> atom */
  char buf[MAX_NAME] = {0};
  int n = 0;
  term_t *cur = deref(env, goal->args[1]);
  while (is_cons(cur)) {
    term_t *head = deref(env, cur->args[0]);
    const char *cs = term_atom_str(head);
    if (!cs || cs[1] != '\0')
      return BUILTIN_FAIL;
    if (n >= MAX_NAME - 1)
      return BUILTIN_FAIL;
    buf[n++] = cs[0];
    cur = deref(env, cur->args[1]);
  }
  if (!is_nil(cur))
    return BUILTIN_FAIL;
  buf[n] = '\0';
  return unify(ctx, goal->args[0], make_const(ctx, buf), env) ? BUILTIN_OK
                                                              : BUILTIN_FAIL;
}

static builtin_result_t builtin_atom_codes(prolog_ctx_t *ctx, term_t *goal,
                                           env_t *env) {
  term_t *atom = deref(env, goal->args[0]);
  if (atom->type != VAR) {
    const char *s = term_atom_str(atom);
    if (!s)
      return BUILTIN_FAIL;
    term_t *list = make_const(ctx, "[]");
    for (int i = (int)strlen(s) - 1; i >= 0; i--) {
      char code[8];
      snprintf(code, sizeof(code), "%d", (unsigned char)s[i]);
      term_t *args[2] = {make_const(ctx, code), list};
      list = make_func(ctx, ".", args, 2);
    }
    return unify(ctx, goal->args[1], list, env) ? BUILTIN_OK : BUILTIN_FAIL;
  }
  /* codes -> atom */
  char buf[MAX_NAME] = {0};
  int n = 0;
  term_t *cur = deref(env, goal->args[1]);
  while (is_cons(cur)) {
    term_t *head = deref(env, cur->args[0]);
    int c;
    if (!term_as_int(head, &c) || c < 0 || c > 255)
      return BUILTIN_FAIL;
    if (n >= MAX_NAME - 1)
      return BUILTIN_FAIL;
    buf[n++] = (char)c;
    cur = deref(env, cur->args[1]);
  }
  if (!is_nil(cur))
    return BUILTIN_FAIL;
  buf[n] = '\0';
  return unify(ctx, goal->args[0], make_const(ctx, buf), env) ? BUILTIN_OK
                                                              : BUILTIN_FAIL;
}

static builtin_result_t builtin_char_code(prolog_ctx_t *ctx, term_t *goal,
                                          env_t *env) {
  term_t *ch = deref(env, goal->args[0]);
  term_t *code = deref(env, goal->args[1]);
  if (ch->type != VAR) {
    const char *s = term_atom_str(ch);
    if (!s || s[1] != '\0')
      return BUILTIN_FAIL;
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", (unsigned char)s[0]);
    return unify(ctx, goal->args[1], make_const(ctx, buf), env) ? BUILTIN_OK
                                                                : BUILTIN_FAIL;
  }
  if (code->type != VAR) {
    int c;
    if (!term_as_int(code, &c) || c < 0 || c > 255)
      return BUILTIN_FAIL;
    char buf[2] = {(char)c, '\0'};
    return unify(ctx, goal->args[0], make_const(ctx, buf), env) ? BUILTIN_OK
                                                                : BUILTIN_FAIL;
  }
  return BUILTIN_FAIL;
}

static builtin_result_t builtin_atom_number(prolog_ctx_t *ctx, term_t *goal,
                                            env_t *env) {
  term_t *atom = deref(env, goal->args[0]);
  term_t *num = deref(env, goal->args[1]);
  if (atom->type != VAR) {
    const char *s = term_atom_str(atom);
    if (!s || !is_integer_str(s))
      return BUILTIN_FAIL;
    return unify(ctx, goal->args[1], make_const(ctx, s), env) ? BUILTIN_OK
                                                              : BUILTIN_FAIL;
  }
  if (num->type != VAR) {
    if (!is_integer_str(num->name))
      return BUILTIN_FAIL;
    return unify(ctx, goal->args[0], make_const(ctx, num->name), env)
               ? BUILTIN_OK
               : BUILTIN_FAIL;
  }
  return BUILTIN_FAIL;
}

static builtin_result_t builtin_number_codes(prolog_ctx_t *ctx, term_t *goal,
                                             env_t *env) {
  term_t *num = deref(env, goal->args[0]);
  if (num->type != VAR) {
    if (!is_integer_str(num->name))
      return BUILTIN_FAIL;
    const char *s = num->name;
    term_t *list = make_const(ctx, "[]");
    for (int i = (int)strlen(s) - 1; i >= 0; i--) {
      char code[8];
      snprintf(code, sizeof(code), "%d", (unsigned char)s[i]);
      term_t *args[2] = {make_const(ctx, code), list};
      list = make_func(ctx, ".", args, 2);
    }
    return unify(ctx, goal->args[1], list, env) ? BUILTIN_OK : BUILTIN_FAIL;
  }
  /* codes -> number: build string then validate */
  char buf[MAX_NAME] = {0};
  int n = 0;
  term_t *cur = deref(env, goal->args[1]);
  while (is_cons(cur)) {
    term_t *head = deref(env, cur->args[0]);
    int c;
    if (!term_as_int(head, &c) || c < 0 || c > 255)
      return BUILTIN_FAIL;
    if (n >= MAX_NAME - 1)
      return BUILTIN_FAIL;
    buf[n++] = (char)c;
    cur = deref(env, cur->args[1]);
  }
  if (!is_nil(cur))
    return BUILTIN_FAIL;
  buf[n] = '\0';
  if (!is_integer_str(buf))
    return BUILTIN_FAIL;
  return unify(ctx, goal->args[0], make_const(ctx, buf), env) ? BUILTIN_OK
                                                              : BUILTIN_FAIL;
}

static builtin_result_t builtin_number_chars(prolog_ctx_t *ctx, term_t *goal,
                                             env_t *env) {
  term_t *num = deref(env, goal->args[0]);
  if (num->type != VAR) {
    if (!is_integer_str(num->name))
      return BUILTIN_FAIL;
    const char *s = num->name;
    term_t *list = make_const(ctx, "[]");
    for (int i = (int)strlen(s) - 1; i >= 0; i--) {
      char ch[2] = {s[i], '\0'};
      term_t *args[2] = {make_const(ctx, ch), list};
      list = make_func(ctx, ".", args, 2);
    }
    return unify(ctx, goal->args[1], list, env) ? BUILTIN_OK : BUILTIN_FAIL;
  }
  /* chars -> number */
  char buf[MAX_NAME] = {0};
  int n = 0;
  term_t *cur = deref(env, goal->args[1]);
  while (is_cons(cur)) {
    term_t *head = deref(env, cur->args[0]);
    const char *cs = term_atom_str(head);
    if (!cs || cs[1] != '\0')
      return BUILTIN_FAIL;
    if (n >= MAX_NAME - 1)
      return BUILTIN_FAIL;
    buf[n++] = cs[0];
    cur = deref(env, cur->args[1]);
  }
  if (!is_nil(cur))
    return BUILTIN_FAIL;
  buf[n] = '\0';
  if (!is_integer_str(buf))
    return BUILTIN_FAIL;
  return unify(ctx, goal->args[0], make_const(ctx, buf), env) ? BUILTIN_OK
                                                              : BUILTIN_FAIL;
}

/* ── term introspection ─────────────────────────────────────────────── */
static builtin_result_t builtin_functor(prolog_ctx_t *ctx, term_t *goal,
                                        env_t *env) {
  term_t *term = deref(env, goal->args[0]);
  term_t *name = goal->args[1];
  term_t *arity = goal->args[2];
  if (term->type != VAR) {
    const char *fname;
    int ar;
    if (term->type == CONST || term->type == STRING) {
      fname = term_atom_str(term);
      ar = 0;
    } else if (term->type == FUNC) {
      fname = term->name;
      ar = term->arity;
    } else {
      return BUILTIN_FAIL;
    }
    char ar_buf[8];
    snprintf(ar_buf, sizeof(ar_buf), "%d", ar);
    return (unify(ctx, name, make_const(ctx, fname), env) &&
            unify(ctx, arity, make_const(ctx, ar_buf), env))
               ? 1
               : -1;
  }
  /* compose */
  term_t *n = deref(env, name);
  term_t *a = deref(env, arity);
  if (n->type == VAR || a->type == VAR)
    return BUILTIN_FAIL;
  const char *fname = term_atom_str(n);
  if (!fname)
    return BUILTIN_FAIL;
  int ar;
  if (!term_as_int(a, &ar) || ar < 0 || ar > MAX_ARGS)
    return BUILTIN_FAIL;
  term_t *t;
  if (ar == 0) {
    t = make_const(ctx, fname);
  } else {
    term_t *args[MAX_ARGS];
    for (int i = 0; i < ar; i++) {
      args[i] = make_var(ctx, NULL, ctx->var_counter++);
    }
    t = make_func(ctx, fname, args, ar);
  }
  return unify(ctx, goal->args[0], t, env) ? BUILTIN_OK : BUILTIN_FAIL;
}

static builtin_result_t builtin_arg(prolog_ctx_t *ctx, term_t *goal,
                                    env_t *env) {
  term_t *n = deref(env, goal->args[0]);
  term_t *term = deref(env, goal->args[1]);
  if (n->type == VAR || term->type != FUNC)
    return BUILTIN_FAIL;
  int idx;
  if (!term_as_int(n, &idx) || idx < 1 || idx > term->arity)
    return BUILTIN_FAIL;
  return unify(ctx, goal->args[2], term->args[idx - 1], env) ? BUILTIN_OK
                                                             : BUILTIN_FAIL;
}

static builtin_result_t builtin_univ(prolog_ctx_t *ctx, term_t *goal,
                                     env_t *env) {
  term_t *term = deref(env, goal->args[0]);
  term_t *list = deref(env, goal->args[1]);
  if (term->type != VAR) {
    term_t *result;
    if (term->type == CONST || term->type == STRING) {
      term_t *args[2] = {term, make_const(ctx, "[]")};
      result = make_func(ctx, ".", args, 2);
    } else if (term->type == FUNC) {
      result = make_const(ctx, "[]");
      for (int i = term->arity - 1; i >= 0; i--) {
        term_t *args[2] = {term->args[i], result};
        result = make_func(ctx, ".", args, 2);
      }
      term_t *args[2] = {make_const(ctx, term->name), result};
      result = make_func(ctx, ".", args, 2);
    } else {
      return BUILTIN_FAIL;
    }
    return unify(ctx, goal->args[1], result, env) ? BUILTIN_OK : BUILTIN_FAIL;
  }
  /* list -> term */
  term_t *cur = deref(env, list);
  if (!is_cons(cur))
    return BUILTIN_FAIL;
  term_t *head = deref(env, cur->args[0]);
  const char *fname = term_atom_str(head);
  if (!fname)
    return BUILTIN_FAIL;
  term_t *args[MAX_ARGS];
  int ar = 0;
  cur = deref(env, cur->args[1]);
  while (is_cons(cur)) {
    if (ar >= MAX_ARGS)
      return BUILTIN_FAIL;
    args[ar++] = deref(env, cur->args[0]);
    cur = deref(env, cur->args[1]);
  }
  if (!is_nil(cur))
    return BUILTIN_FAIL;
  term_t *result =
      (ar == 0) ? make_const(ctx, fname) : make_func(ctx, fname, args, ar);
  return unify(ctx, goal->args[0], result, env) ? BUILTIN_OK : BUILTIN_FAIL;
}

static builtin_result_t builtin_copy_term(prolog_ctx_t *ctx, term_t *goal,
                                          env_t *env) {
  term_t *orig = substitute(ctx, env, deref(env, goal->args[0]));
  term_t *copy = rename_vars(ctx, orig);
  return unify(ctx, goal->args[1], copy, env) ? BUILTIN_OK : BUILTIN_FAIL;
}

static void flatten_body(env_t *env, term_t *body, clause_t *c) {
  while (body->type == FUNC && strcmp(body->name, ",") == 0 &&
         body->arity == 2 && c->body_count < MAX_GOALS - 1) {
    c->body[c->body_count++] = deref(env, body->args[0]);
    body = deref(env, body->args[1]);
  }
  c->body[c->body_count++] = body;
}

static builtin_result_t builtin_assertz(prolog_ctx_t *ctx, term_t *goal,
                                        env_t *env) {
  if (ctx->db_count >= MAX_CLAUSES)
    return BUILTIN_FAIL;
  term_t *arg = substitute(ctx, env, deref(env, goal->args[0]));
  clause_t *c = &ctx->database[ctx->db_count];
  c->body_count = 0;
  if (arg->type == FUNC && strcmp(arg->name, ":-") == 0 && arg->arity == 2) {
    c->head = deref(env, arg->args[0]);
    flatten_body(env, deref(env, arg->args[1]), c);
  } else {
    c->head = arg;
  }
  ctx->db_count++;
  return BUILTIN_OK;
}

static builtin_result_t builtin_asserta(prolog_ctx_t *ctx, term_t *goal,
                                        env_t *env) {
  if (ctx->db_count >= MAX_CLAUSES)
    return BUILTIN_FAIL;
  for (int i = ctx->db_count; i > 0; i--)
    ctx->database[i] = ctx->database[i - 1];
  ctx->db_count++;
  term_t *arg = substitute(ctx, env, deref(env, goal->args[0]));
  clause_t *c = &ctx->database[0];
  c->body_count = 0;
  if (arg->type == FUNC && strcmp(arg->name, ":-") == 0 && arg->arity == 2) {
    c->head = deref(env, arg->args[0]);
    flatten_body(env, deref(env, arg->args[1]), c);
  } else {
    c->head = arg;
  }
  return BUILTIN_OK;
}

static builtin_result_t builtin_retract(prolog_ctx_t *ctx, term_t *goal,
                                        env_t *env) {
  term_t *arg = deref(env, goal->args[0]);
  // accept retract(Head) or retract((Head :- Body)) — body not checked
  term_t *head_pat =
      (arg->type == FUNC && strcmp(arg->name, ":-") == 0 && arg->arity == 2)
          ? deref(env, arg->args[0])
          : arg;
  for (int i = 0; i < ctx->db_count; i++) {
    int env_mark = env->count;
    int trm_save = ctx->term_count;
    if (unify(ctx, head_pat, rename_vars(ctx, ctx->database[i].head), env)) {
      for (int j = i; j < ctx->db_count - 1; j++)
        ctx->database[j] = ctx->database[j + 1];
      ctx->db_count--;
      return BUILTIN_OK;
    }
    env->count = env_mark;
    ctx->term_count = trm_save;
  }
  return BUILTIN_FAIL;
}

static builtin_result_t builtin_retractall(prolog_ctx_t *ctx, term_t *goal,
                                           env_t *env) {
  term_t *head_pat = deref(env, goal->args[0]);
  int i = 0;
  while (i < ctx->db_count) {
    int env_mark = env->count;
    int trm_save = ctx->term_count;
    bool matched =
        unify(ctx, head_pat, rename_vars(ctx, ctx->database[i].head), env);
    env->count = env_mark;
    ctx->term_count = trm_save;
    if (matched) {
      for (int j = i; j < ctx->db_count - 1; j++)
        ctx->database[j] = ctx->database[j + 1];
      ctx->db_count--;
    } else {
      i++;
    }
  }
  return BUILTIN_OK; /* always succeeds */
}

/* ── arithmetic helpers ─────────────────────────────────────────────── */
static builtin_result_t builtin_succ(prolog_ctx_t *ctx, term_t *goal,
                                     env_t *env) {
  term_t *x = deref(env, goal->args[0]);
  term_t *y = deref(env, goal->args[1]);
  char buf[16];
  int n;
  if (x->type != VAR) {
    if (!term_as_int(x, &n) || n < 0)
      return BUILTIN_FAIL;
    snprintf(buf, sizeof(buf), "%d", n + 1);
    return unify(ctx, goal->args[1], make_const(ctx, buf), env) ? BUILTIN_OK
                                                                : BUILTIN_FAIL;
  }
  if (y->type != VAR) {
    if (!term_as_int(y, &n) || n < 1)
      return BUILTIN_FAIL;
    snprintf(buf, sizeof(buf), "%d", n - 1);
    return unify(ctx, goal->args[0], make_const(ctx, buf), env) ? BUILTIN_OK
                                                                : BUILTIN_FAIL;
  }
  return BUILTIN_FAIL;
}

static builtin_result_t builtin_plus(prolog_ctx_t *ctx, term_t *goal,
                                     env_t *env) {
  term_t *x = deref(env, goal->args[0]);
  term_t *y = deref(env, goal->args[1]);
  term_t *z = deref(env, goal->args[2]);
  char buf[16];
  int nx, ny, nz;
  if (x->type != VAR && y->type != VAR) {
    if (!term_as_int(x, &nx) || !term_as_int(y, &ny))
      return BUILTIN_FAIL;
    snprintf(buf, sizeof(buf), "%d", nx + ny);
    return unify(ctx, goal->args[2], make_const(ctx, buf), env) ? BUILTIN_OK
                                                                : BUILTIN_FAIL;
  }
  if (x->type != VAR && z->type != VAR) {
    if (!term_as_int(x, &nx) || !term_as_int(z, &nz))
      return BUILTIN_FAIL;
    snprintf(buf, sizeof(buf), "%d", nz - nx);
    return unify(ctx, goal->args[1], make_const(ctx, buf), env) ? BUILTIN_OK
                                                                : BUILTIN_FAIL;
  }
  if (y->type != VAR && z->type != VAR) {
    if (!term_as_int(y, &ny) || !term_as_int(z, &nz))
      return BUILTIN_FAIL;
    snprintf(buf, sizeof(buf), "%d", nz - ny);
    return unify(ctx, goal->args[0], make_const(ctx, buf), env) ? BUILTIN_OK
                                                                : BUILTIN_FAIL;
  }
  return BUILTIN_FAIL;
}

static builtin_result_t builtin_make(prolog_ctx_t *ctx, term_t *goal,
                                     env_t *env) {
  (void)goal;
  (void)env;
  if (ctx->make_file_count == 0)
    return BUILTIN_OK; // no files tracked yet

  // check which files have changed since they were loaded
  int changed = 0;
  for (int i = 0; i < ctx->make_file_count; i++) {
    long long cur = prolog_file_mtime(ctx->make_files[i]);
    if (cur == -1LL || cur != ctx->make_file_mtimes[i])
      changed++;
  }

  if (changed == 0) {
    io_writef(ctx, "%% make: nothing to reload\n");
    return BUILTIN_OK;
  }

  // snapshot file list before resetting pools
  char files[MAX_MAKE_FILES][MAX_FILE_PATH];
  int count = ctx->make_file_count;
  for (int i = 0; i < count; i++) {
    strncpy(files[i], ctx->make_files[i], MAX_FILE_PATH - 1);
    files[i][MAX_FILE_PATH - 1] = '\0';
  }

  // roll back to the state captured before the first consult
  ctx->db_count = ctx->make_db_mark;
  ctx->term_count = ctx->make_term_mark;
  ctx->string_pool_offset = ctx->make_string_mark;
  ctx->make_file_count = 0;

  for (int i = 0; i < count; i++)
    prolog_load_file(ctx, files[i]);

  io_writef(ctx, "%% make: reloaded %d file(s) (%d changed)\n", count, changed);
  return BUILTIN_OK;
}

static const builtin_t builtins[] = {
    // 0-arity
    {"true", 0, builtin_true},
    {"fail", 0, builtin_fail},
    {"!", 0, builtin_cut},
    {"stats", 0, builtin_stats},
    {"make", 0, builtin_make},
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
    {"once", 1, builtin_once},
    {"\\+", 1, builtin_not},
    {"var", 1, builtin_var},
    {"nonvar", 1, builtin_nonvar},
    {"atom", 1, builtin_atom},
    {"integer", 1, builtin_integer},
    {"is_list", 1, builtin_is_list},
    {"nl", 0, builtin_nl},
    {"write", 1, builtin_write},
    {"writeln", 1, builtin_writeln},
    {"writeq", 1, builtin_writeq},
    {"consult", 1, builtin_consult},
    {"include", 1, builtin_include},
    // 3-arity
    {"findall", 3, builtin_findall},
    {"bagof", 3, builtin_bagof},
    // type checks
    {"compound", 1, builtin_compound},
    {"callable", 1, builtin_callable},
    {"number", 1, builtin_number},
    {"atomic", 1, builtin_atomic},
    {"string", 1, builtin_string},
    // atom / string
    {"atom_length", 2, builtin_atom_length},
    {"atom_concat", 3, builtin_atom_concat},
    {"atom_chars", 2, builtin_atom_chars},
    {"atom_codes", 2, builtin_atom_codes},
    {"char_code", 2, builtin_char_code},
    {"atom_number", 2, builtin_atom_number},
    {"number_codes", 2, builtin_number_codes},
    {"number_chars", 2, builtin_number_chars},
    // term introspection
    {"functor", 3, builtin_functor},
    {"arg", 3, builtin_arg},
    {"=..", 2, builtin_univ},
    {"copy_term", 2, builtin_copy_term},
    // dynamic database
    {"assertz", 1, builtin_assertz},
    {"asserta", 1, builtin_asserta},
    {"retract", 1, builtin_retract},
    {"retractall", 1, builtin_retractall},
    // arithmetic helpers
    {"succ", 2, builtin_succ},
    {"plus", 3, builtin_plus},
    {NULL, 0, NULL}};

builtin_result_t try_builtin(prolog_ctx_t *ctx, term_t *goal, env_t *env) {
  goal = deref(env, goal);

  const char *name = goal->name;
  int arity = (goal->type == CONST)  ? 0
              : (goal->type == FUNC) ? goal->arity
                                     : -1;

  if (arity < 0)
    return BUILTIN_NOT_HANDLED;

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

  return BUILTIN_NOT_HANDLED;
}
