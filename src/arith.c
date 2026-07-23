#include "platform_impl.h"

#include <limits.h>
#include <math.h>

//****
//* arithmetic operator table (int-only ops: bitwise/mod/floor-div)
//****

typedef struct {
  const char *op;
  int (*fn)(int, int);
} arith_op_t;

static int arith_div(int a, int b) { return b ? a / b : 0; }
static int arith_mod(int a, int b) {
  if (!b)
    return 0;
  int r = a % b;
  // iso: result has sign of divisor
  if (r != 0 && (r ^ b) < 0)
    r += b;
  return r;
}
static int arith_bor(int a, int b) { return a | b; }
static int arith_band(int a, int b) { return a & b; }
static int arith_xor(int a, int b) { return a ^ b; }
static int arith_shr(int a, int b) { return (int)((unsigned)a >> b); }
static int arith_shl(int a, int b) { return (int)((unsigned)a << b); }

static const arith_op_t arith_int_ops[] = {
    {"mod", arith_mod},  {"//", arith_div},  {"\\/", arith_bor},
    {"/\\", arith_band}, {"xor", arith_xor}, {">>", arith_shr},
    {"<<", arith_shl},   {NULL, NULL}};

static bool is_int_only_op(const char *name) {
  for (const arith_op_t *op = arith_int_ops; op->op; op++)
    if (strcmp(name, op->op) == 0)
      return true;
  return false;
}

//****
//* arith_val_t helpers
//****

static double av_dbl(arith_val_t v) { return v.is_float ? v.f : (double)v.i; }

static term_t *av_to_term(trilog_ctx_t *ctx, arith_val_t v) {
  return v.is_float ? make_float(ctx, v.f) : make_int(ctx, v.i);
}

// range-check a double before narrowing to this engine's 32-bit int.
static bool av_to_int_result(trilog_ctx_t *ctx, double r, arith_val_t *result,
                             const char *pred) {
  if (r < (double)INT_MIN || r > (double)INT_MAX) {
    throw_evaluation_error(ctx, "int_overflow", pred);
    return false;
  }
  result->is_float = false;
  result->i = (int)r;
  return true;
}

//****
//* arithmetic evaluator
//****

bool eval_arith(trilog_ctx_t *ctx, term_t *t, env_t *env, arith_val_t *result,
                const char *pred) {
  t = deref(env, t);

  if (term_as_int(t, &result->i)) {
    result->is_float = false;
    return true;
  }
  if (term_as_float(t, &result->f)) {
    result->is_float = true;
    return true;
  }

  if (t->type == VAR) {
    throw_instantiation_error(ctx, pred);
    return false;
  }

  if (t->type == FUNC && t->arity == 1) {
    arith_val_t val;
    if (!eval_arith(ctx, t->args[0], env, &val, pred))
      return false;

    if (strcmp(t->name, "-") == 0) {
      if (val.is_float) {
        result->is_float = true;
        result->f = -val.f;
      } else {
        result->is_float = false;
        result->i = -val.i;
      }
      return true;
    }
    if (strcmp(t->name, "abs") == 0) {
      if (val.is_float) {
        result->is_float = true;
        result->f = fabs(val.f);
      } else {
        result->is_float = false;
        result->i = val.i < 0 ? -val.i : val.i;
      }
      return true;
    }
    if (strcmp(t->name, "\\") == 0) {
      if (val.is_float) {
        throw_type_error(ctx, "integer", make_float(ctx, val.f), pred);
        return false;
      }
      result->is_float = false;
      result->i = ~val.i;
      return true;
    }
    if (strcmp(t->name, "floor") == 0)
      return av_to_int_result(ctx, floor(av_dbl(val)), result, pred);
    if (strcmp(t->name, "ceiling") == 0)
      return av_to_int_result(ctx, ceil(av_dbl(val)), result, pred);
    if (strcmp(t->name, "round") == 0)
      return av_to_int_result(ctx, round(av_dbl(val)), result, pred);
    if (strcmp(t->name, "truncate") == 0)
      return av_to_int_result(ctx, trunc(av_dbl(val)), result, pred);
    if (strcmp(t->name, "float") == 0) {
      result->is_float = true;
      result->f = av_dbl(val);
      return true;
    }
    throw_evaluable_error(ctx, t->name, 1, pred);
    return false;
  }

  if (t->type == FUNC && t->arity == 2) {
    arith_val_t left, right;
    if (!eval_arith(ctx, t->args[0], env, &left, pred))
      return false;
    if (!eval_arith(ctx, t->args[1], env, &right, pred))
      return false;

    bool either_float = left.is_float || right.is_float;

    if (is_int_only_op(t->name) && either_float) {
      arith_val_t bad = left.is_float ? left : right;
      throw_type_error(ctx, "integer", make_float(ctx, bad.f), pred);
      return false;
    }

    if (strcmp(t->name, "+") == 0) {
      if (either_float) {
        result->is_float = true;
        result->f = av_dbl(left) + av_dbl(right);
        return true;
      }
      result->is_float = false;
      if (__builtin_add_overflow(left.i, right.i, &result->i)) {
        throw_evaluation_error(ctx, "int_overflow", pred);
        return false;
      }
      return true;
    }
    if (strcmp(t->name, "-") == 0) {
      if (either_float) {
        result->is_float = true;
        result->f = av_dbl(left) - av_dbl(right);
        return true;
      }
      result->is_float = false;
      if (__builtin_sub_overflow(left.i, right.i, &result->i)) {
        throw_evaluation_error(ctx, "int_overflow", pred);
        return false;
      }
      return true;
    }
    if (strcmp(t->name, "*") == 0) {
      if (either_float) {
        result->is_float = true;
        result->f = av_dbl(left) * av_dbl(right);
        return true;
      }
      result->is_float = false;
      if (__builtin_mul_overflow(left.i, right.i, &result->i)) {
        throw_evaluation_error(ctx, "int_overflow", pred);
        return false;
      }
      return true;
    }
    if (strcmp(t->name, "/") == 0) {
      if (either_float) {
        if (av_dbl(right) == 0.0) {
          throw_evaluation_error(ctx, "zero_divisor", pred);
          return false;
        }
        result->is_float = true;
        result->f = av_dbl(left) / av_dbl(right);
        return true;
      }
      if (right.i == 0) {
        throw_evaluation_error(ctx, "zero_divisor", pred);
        return false;
      }
      result->is_float = false;
      result->i = arith_div(left.i, right.i);
      return true;
    }
    if (strcmp(t->name, "max") == 0) {
      if (either_float) {
        result->is_float = true;
        result->f = av_dbl(left) > av_dbl(right) ? av_dbl(left) : av_dbl(right);
      } else {
        result->is_float = false;
        result->i = left.i > right.i ? left.i : right.i;
      }
      return true;
    }
    if (strcmp(t->name, "min") == 0) {
      if (either_float) {
        result->is_float = true;
        result->f = av_dbl(left) < av_dbl(right) ? av_dbl(left) : av_dbl(right);
      } else {
        result->is_float = false;
        result->i = left.i < right.i ? left.i : right.i;
      }
      return true;
    }

    // remaining ops are all int-only (checked above), and already have
    // both operands narrowed to int since either_float was false here.
    if ((strcmp(t->name, "//") == 0 || strcmp(t->name, "mod") == 0) &&
        right.i == 0) {
      throw_evaluation_error(ctx, "zero_divisor", pred);
      return false;
    }
    for (const arith_op_t *op = arith_int_ops; op->op; op++) {
      if (strcmp(t->name, op->op) == 0) {
        result->is_float = false;
        result->i = op->fn(left.i, right.i);
        return true;
      }
    }
  }

  // unknown functor or non-numeric atom: type_error(evaluable, name/arity)
  if (t->type == CONST || t->type == FUNC) {
    int arity = (t->type == FUNC) ? t->arity : 0;
    throw_evaluable_error(ctx, t->name, arity, pred);
  } else {
    throw_type_error(ctx, "evaluable", t, pred);
  }
  return false;
}

//****
//* arithmetic builtins
//****

builtin_result_t builtin_is(trilog_ctx_t *ctx, term_t *goal, env_t *env) {
  arith_val_t result;
  if (!eval_arith(ctx, goal->args[1], env, &result, "is/2"))
    return ctx->has_runtime_error ? BUILTIN_ERROR : BUILTIN_FAIL;
  term_t *result_term = av_to_term(ctx, result);
  return unify(ctx, goal->args[0], result_term, env) ? BUILTIN_OK
                                                     : BUILTIN_FAIL;
}

#define ARITH_CMP_BUILTIN(name, op, pred)                                      \
  builtin_result_t name(trilog_ctx_t *ctx, term_t *goal, env_t *env) {         \
    arith_val_t left, right;                                                   \
    if (!eval_arith(ctx, goal->args[0], env, &left, pred))                     \
      return ctx->has_runtime_error ? BUILTIN_ERROR : BUILTIN_NOT_HANDLED;     \
    if (!eval_arith(ctx, goal->args[1], env, &right, pred))                    \
      return ctx->has_runtime_error ? BUILTIN_ERROR : BUILTIN_NOT_HANDLED;     \
    return av_dbl(left) op av_dbl(right) ? BUILTIN_OK : BUILTIN_FAIL;          \
  }

ARITH_CMP_BUILTIN(builtin_lt, <, "</2")
ARITH_CMP_BUILTIN(builtin_gt, >, ">/2")
ARITH_CMP_BUILTIN(builtin_le, <=, "=</2")
ARITH_CMP_BUILTIN(builtin_ge, >=, ">=/2")
ARITH_CMP_BUILTIN(builtin_arith_eq, ==, "=:=/2")
ARITH_CMP_BUILTIN(builtin_arith_ne, !=, "=\\=/2")

#undef ARITH_CMP_BUILTIN
