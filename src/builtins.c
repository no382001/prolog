#include "prolog.h"
#include <stdlib.h>
#include <string.h>

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

    if (strcmp(t->name, "+") == 0) {
      *result = left + right;
      return true;
    }
    if (strcmp(t->name, "-") == 0) {
      *result = left - right;
      return true;
    }
    if (strcmp(t->name, "*") == 0) {
      *result = left * right;
      return true;
    }
    if (strcmp(t->name, "/") == 0) {
      *result = right ? left / right : 0;
      return true;
    }
    if (strcmp(t->name, "mod") == 0) {
      *result = right ? left % right : 0;
      return true;
    }
  }

  return false;
}

int try_builtin(prolog_ctx_t *ctx, term_t *goal, env_t *env) {
  goal = deref(env, goal);

  // handle 0-arity builtins (parsed as CONST)
  if (goal->type == CONST) {
    if (strcmp(goal->name, "true") == 0) {
      return 1;
    }
    if (strcmp(goal->name, "fail") == 0) {
      return -1;
    }
    return 0; // not a builtin constant
  }

  if (goal->type != FUNC)
    return 0;

  // is(X, Expr) - arithmetic evaluation
  if (strcmp(goal->name, "is") == 0 && goal->arity == 2) {
    int result;
    if (!eval_arith(ctx, goal->args[1], env, &result))
      return -1;

    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    term_t *result_term = make_const(ctx, buf);

    return unify(ctx, goal->args[0], result_term, env) ? 1 : -1;
  }

  // comparison operators
  if (goal->arity == 2) {
    int left, right;
    bool have_left = eval_arith(ctx, goal->args[0], env, &left);
    bool have_right = eval_arith(ctx, goal->args[1], env, &right);

    if (have_left && have_right) {
      if (strcmp(goal->name, "<") == 0)
        return left < right ? 1 : -1;
      if (strcmp(goal->name, ">") == 0)
        return left > right ? 1 : -1;
      if (strcmp(goal->name, "=<") == 0)
        return left <= right ? 1 : -1;
      if (strcmp(goal->name, ">=") == 0)
        return left >= right ? 1 : -1;
      if (strcmp(goal->name, "=:=") == 0)
        return left == right ? 1 : -1;
      if (strcmp(goal->name, "=\\=") == 0)
        return left != right ? 1 : -1;
    }
  }

  // =/2 - unification
  if (strcmp(goal->name, "=") == 0 && goal->arity == 2) {
    return unify(ctx, goal->args[0], goal->args[1], env) ? 1 : -1;
  }

  // \=/2 - not unifiable
  if (strcmp(goal->name, "\\=") == 0 && goal->arity == 2) {
    int old_count = env->count;
    bool unified = unify(ctx, goal->args[0], goal->args[1], env);
    env->count = old_count;
    return unified ? -1 : 1;
  }

  return 0; // not a builtin
}