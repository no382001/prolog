#include "platform_impl.h"

bool son(prolog_ctx_t *ctx, goal_stmt_t *cn, int *clause_idx, env_t *env,
         int env_mark, goal_stmt_t *resolvent) {
  assert(ctx != NULL && "Context is NULL");
  assert(cn != NULL && "Current node is NULL");
  assert(clause_idx != NULL && "Clause index pointer is NULL");
  assert(env != NULL && "Environment is NULL");
  assert(resolvent != NULL && "Resolvent is NULL");
  assert(env_mark >= 0 && env_mark <= env->count && "Invalid env_mark");

  ctx->stats.son_calls++;
  if (cn->count == 0)
    return false;

  term_t *selected_goal = cn->goals[0];
  assert(selected_goal != NULL && "Selected goal is NULL");

  debug(ctx, "\n=== SON: looking for match ===\n");
  debug(ctx, "GOAL: ");
  debug_term_raw(ctx, selected_goal);
  debug(ctx, "\n");
  debug(ctx, "Starting from clause %d\n", *clause_idx);

  if (*clause_idx == 0) {
    int builtin_result = try_builtin(ctx, selected_goal, env);
    if (builtin_result != 0) {
      if (builtin_result == 1) {
        debug(ctx, ">>> BUILTIN succeeded!\n");
        resolvent->count = 0;
        for (int j = 1; j < cn->count; j++) {
          assert(resolvent->count < MAX_GOALS && "Resolvent overflow");
          resolvent->goals[resolvent->count++] = cn->goals[j];
        }
        *clause_idx = ctx->db_count; // prevent backtracking into clauses
        return true;
      } else {
        debug(ctx, ">>> BUILTIN failed\n");
        return false;
      }
    }
  }

  for (int i = *clause_idx; i < ctx->db_count; i++) {
    clause_t *c = &ctx->database[i];
    assert(c->head != NULL && "Clause head is NULL");

    env->count = env_mark;

    int id = ++ctx->var_counter;
    int term_save = ctx->term_count; // reclaim on failed unification
    term_t *renamed_head = rename_vars(ctx, c->head, id);
    assert(renamed_head != NULL && "Failed to rename clause head");

    debug(ctx, "\n--- Trying clause %d ---\n", i);
    debug(ctx, "CLAUSE HEAD (renamed): ");
    debug_term_raw(ctx, renamed_head);
    debug(ctx, "\n");

    if (unify(ctx, selected_goal, renamed_head, env)) {
      debug(ctx, ">>> UNIFIED! Building resolvent...\n");

      resolvent->count = 0;

      for (int j = 0; j < c->body_count; j++) {
        assert(resolvent->count < MAX_GOALS && "Resolvent overflow");
        resolvent->goals[resolvent->count++] = rename_vars(ctx, c->body[j], id);
      }

      for (int j = 1; j < cn->count; j++) {
        assert(resolvent->count < MAX_GOALS && "Resolvent overflow");
        resolvent->goals[resolvent->count++] = cn->goals[j];
      }

      if (ctx->debug_enabled) {
        debug(ctx, "RESOLVENT has %d goals\n", resolvent->count);
        for (int j = 0; j < resolvent->count; j++) {
          debug(ctx, "  RESOLVENT[%d]: ", j);
          debug_term_raw(ctx, resolvent->goals[j]);
          debug(ctx, "\n");
        }
      }

      *clause_idx = i + 1;
      return true;
    }
    ctx->term_count = term_save;
    debug(ctx, "--- Clause %d failed ---\n", i);
  }

  debug(ctx, "=== SON: no match found ===\n");
  env->count = env_mark;
  return false;
}

static bool has_more_alternatives(prolog_ctx_t *ctx, term_t *goal, env_t *env,
                                  int from_clause) {
  goal = deref(env, goal);
  for (int i = from_clause; i < ctx->db_count; i++) {
    clause_t *c = &ctx->database[i];
    // cheap arity/name check before trying unify
    int goal_arity = (goal->type == FUNC) ? goal->arity : 0;
    int head_arity = (c->head->type == FUNC) ? c->head->arity : 0;
    if (strcmp(goal->name, c->head->name) == 0 && goal_arity == head_arity) {
      return true;
    }
  }
  return false;
}

bool solve_all(prolog_ctx_t *ctx, goal_stmt_t *initial_goals, env_t *env,
               solution_callback_t callback, void *userdata) {
  assert(ctx != NULL && "Context is NULL");
  assert(initial_goals != NULL && "Initial goals is NULL");
  assert(env != NULL && "Environment is NULL");

  frame_t stack[MAX_STACK];
  int sp = 0;

  goal_stmt_t cn = *initial_goals;

  stack[sp].goals = cn;
  stack[sp].clause_index = 0;
  stack[sp].env_mark = env->count;
  stack[sp].cut_point = 0;
  sp++;

  int clause_idx;
  int env_mark;
  int cut_point = 0;
  bool found_any = false;

A:
  if (ctx->debug_enabled) {
    debug(ctx, "\n*** LABEL A: cn has %d goals ***\n", cn.count);
    for (int i = 0; i < cn.count; i++) {
      debug(ctx, "  cn.goals[%d]: ", i);
      debug_term_raw(ctx, cn.goals[i]);
      debug(ctx, "\n");
    }
  }

  if (cn.count == 0) {
    debug(ctx, "*** SOLUTION FOUND ***\n");
    found_any = true;

    if (callback) {
      if (!callback(ctx, env, userdata)) {
        // callback says stop
        return true;
      }
    } else {
      // no callback, just return first solution
      return true;
    }
    // continue to find more solutions
    goto C;
  }

  term_t *first_goal = deref(env, cn.goals[0]);
  if (first_goal->type == CONST && strcmp(first_goal->name, "!") == 0) {
    debug(ctx, "*** CUT executed, pruning stack to %d ***\n", cut_point);
    sp = cut_point + 1;
    goal_stmt_t new_cn = {0};
    for (int i = 1; i < cn.count; i++) {
      new_cn.goals[new_cn.count++] = cn.goals[i];
    }
    cn = new_cn;
    goto A;
  }

  clause_idx = 0;
  env_mark = env->count;

B:
  debug(ctx, "\n*** LABEL B: trying son, clause_idx=%d, env_mark=%d ***\n",
        clause_idx, env_mark);
  {
    goal_stmt_t resolvent;
    if (son(ctx, &cn, &clause_idx, env, env_mark, &resolvent)) {
      assert(sp < MAX_STACK && "Stack overflow");
      debug(ctx, "*** SON succeeded, pushing frame, sp=%d ***\n", sp);

      if (has_more_alternatives(ctx, cn.goals[0], env, clause_idx)) {
        assert(sp < MAX_STACK && "Stack overflow");
        stack[sp].goals = cn;
        stack[sp].clause_index = clause_idx;
        stack[sp].env_mark = env_mark;
        stack[sp].cut_point = cut_point;
        sp++;
        cut_point = sp - 1;
      } // else reuse the stack frame

      cn = resolvent;
      goto A;
    } else {
      debug(ctx, "*** SON failed, going to C ***\n");
      goto C;
    }
  }

C:
  ctx->stats.backtracks++;
  debug(ctx, "\n*** LABEL C: backtracking, sp=%d ***\n", sp);
  sp--;
  if (sp <= 0) {
    debug(ctx, "*** NO MORE SOLUTIONS ***\n");
    return found_any;
  }

  assert(sp > 0 && sp < MAX_STACK && "Invalid stack pointer");

  cn = stack[sp].goals;
  clause_idx = stack[sp].clause_index;
  env_mark = stack[sp].env_mark;
  cut_point = stack[sp].cut_point;

  assert(env_mark >= 0 && env_mark <= env->count &&
         "Invalid env_mark from stack");
  env->count = env_mark;

  debug(ctx, "*** Restored: clause_idx=%d, env_mark=%d, cut_point=%d ***\n",
        clause_idx, env_mark, cut_point);
  goto B;
}

bool solve(prolog_ctx_t *ctx, goal_stmt_t *initial_goals, env_t *env) {
  return solve_all(ctx, initial_goals, env, NULL, NULL);
}