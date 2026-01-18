#include "prolog.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
  const char *op;
  int (*fn)(int, int);
} arith_op_t;

static int arith_add(int a, int b) { return a + b; }
static int arith_sub(int a, int b) { return a - b; }
static int arith_mul(int a, int b) { return a * b; }
static int arith_div(int a, int b) { return b ? a / b : 0; }
static int arith_mod(int a, int b) { return b ? a % b : 0; }

static const arith_op_t arith_ops[] = {
  {"+",   arith_add},
  {"-",   arith_sub},
  {"*",   arith_mul},
  {"/",   arith_div},
  {"mod", arith_mod},
  {NULL, NULL}
};

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
  int arity;          // -1 means any arity, 0 for CONST
  int (*handler)(prolog_ctx_t *ctx, term_t *goal, env_t *env);
} builtin_t;

static int builtin_true(prolog_ctx_t *ctx, term_t *goal, env_t *env) {
  (void)ctx; (void)goal; (void)env;
  return 1;
}

static int builtin_fail(prolog_ctx_t *ctx, term_t *goal, env_t *env) {
  (void)ctx; (void)goal; (void)env;
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
  if (!eval_arith(ctx, goal->args[0], env, &left)) return 0;
  if (!eval_arith(ctx, goal->args[1], env, &right)) return 0;
  return left < right ? 1 : -1;
}

static int builtin_gt(prolog_ctx_t *ctx, term_t *goal, env_t *env) {
  int left, right;
  if (!eval_arith(ctx, goal->args[0], env, &left)) return 0;
  if (!eval_arith(ctx, goal->args[1], env, &right)) return 0;
  return left > right ? 1 : -1;
}

static int builtin_le(prolog_ctx_t *ctx, term_t *goal, env_t *env) {
  int left, right;
  if (!eval_arith(ctx, goal->args[0], env, &left)) return 0;
  if (!eval_arith(ctx, goal->args[1], env, &right)) return 0;
  return left <= right ? 1 : -1;
}

static int builtin_ge(prolog_ctx_t *ctx, term_t *goal, env_t *env) {
  int left, right;
  if (!eval_arith(ctx, goal->args[0], env, &left)) return 0;
  if (!eval_arith(ctx, goal->args[1], env, &right)) return 0;
  return left >= right ? 1 : -1;
}

static int builtin_arith_eq(prolog_ctx_t *ctx, term_t *goal, env_t *env) {
  int left, right;
  if (!eval_arith(ctx, goal->args[0], env, &left)) return 0;
  if (!eval_arith(ctx, goal->args[1], env, &right)) return 0;
  return left == right ? 1 : -1;
}

static int builtin_arith_ne(prolog_ctx_t *ctx, term_t *goal, env_t *env) {
  int left, right;
  if (!eval_arith(ctx, goal->args[0], env, &left)) return 0;
  if (!eval_arith(ctx, goal->args[1], env, &right)) return 0;
  return left != right ? 1 : -1;
}

static const builtin_t builtins[] = {
  // 0-arity (CONST)
  {"true",  0, builtin_true},
  {"fail",  0, builtin_fail},
  // 2-arity
  {"is",    2, builtin_is},
  {"=",     2, builtin_unify},
  {"\\=",   2, builtin_not_unify},
  {"<",     2, builtin_lt},
  {">",     2, builtin_gt},
  {"=<",    2, builtin_le},
  {">=",    2, builtin_ge},
  {"=:=",   2, builtin_arith_eq},
  {"=\\=",  2, builtin_arith_ne},
  {NULL, 0, NULL}
};

int try_builtin(prolog_ctx_t *ctx, term_t *goal, env_t *env) {
  goal = deref(env, goal);

  const char *name = goal->name;
  int arity = (goal->type == CONST) ? 0 : 
              (goal->type == FUNC) ? goal->arity : -1;

  if (arity < 0)
    return 0;

  for (const builtin_t *b = builtins; b->name; b++) {
    if (strcmp(name, b->name) == 0 && arity == b->arity) {
      return b->handler(ctx, goal, env);
    }
  }

  return 0;
}