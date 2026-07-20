#include "platform_impl.h"

//****
//* last-call optimization
//****

// check if lco can safely reclaim bindings in [from, to).
// unsafe when any named (query) var is in the range — reclaiming would
// lose assignments needed by print_bindings at solution time.
static bool lco_safe(env_t *env, int from, int to) {
  for (int i = from; i < to; i++) {
    if (env->bindings[i].name)
      return false; // named query var: must keep for print_bindings
  }
  return true;
}

//****
//* clause selection (son)
//****

bool son(trilog_ctx_t *ctx, goal_stmt_t *cn, int *clause_idx, env_t *env,
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
    builtin_result_t builtin_result = try_builtin(ctx, selected_goal, env);
    if (builtin_result != BUILTIN_NOT_HANDLED) {
      if (builtin_result == BUILTIN_OK) {
        debug(ctx, ">>> BUILTIN succeeded!\n");
        int n = cn->count - 1;
        *resolvent = goals_alloc(ctx, n > 0 ? n : 0);
        if (n > 0 && !resolvent->goals)
          return false;
        for (int j = 1; j < cn->count; j++)
          resolvent->goals[resolvent->count++] = cn->goals[j];
        *clause_idx = -1; // builtin match — skip lco, no backtrack
        return true;
      } else {
        debug(ctx, ">>> BUILTIN failed\n");
        return false;
      }
    }
    // existence_error(procedure, Name/Arity) for truly undefined predicates
    term_t *g = deref(env, selected_goal);
    if (g->type == CONST || g->type == FUNC) {
      const char *gname = g->name;
      int garity = (g->type == FUNC) ? g->arity : 0;
      bool has_clause = false;
      for (int i = 0; i < ctx->db_count && !has_clause; i++) {
        term_t *h = ctx->database[i].head;
        int ha = (h->type == FUNC) ? h->arity : 0;
        if (strcmp(h->name, gname) == 0 && ha == garity)
          has_clause = true;
      }
      bool is_dyn = false;
      for (int i = 0; i < ctx->dynamic_pred_count && !is_dyn; i++) {
        if (ctx->dynamic_preds[i].arity == garity &&
            strcmp(ctx->dynamic_preds[i].name, gname) == 0)
          is_dyn = true;
      }
      if (!has_clause && !is_dyn) {
        char ctx_buf[MAX_NAME + 24];
        snprintf(ctx_buf, sizeof(ctx_buf), "%s/%d", gname, garity);
        term_t *slash_args[2] = {make_const(ctx, gname), make_int(ctx, garity)};
        term_t *indicator = make_func(ctx, "/", slash_args, 2);
        throw_existence_error(ctx, "procedure", indicator, ctx_buf);
        return false;
      }
    }
  }

  for (int i = *clause_idx; i < ctx->db_count; i++) {
    clause_t *c = &ctx->database[i];
    assert(c->head != NULL && "Clause head is NULL");

    env->count = ctx->bind_count = env_mark;

    int term_save = ctx->term_pool_offset; // reclaim on failed unification
    var_id_map_t map = {0};
    term_t *renamed_head = rename_vars_mapped(ctx, c->head, &map);
    if (!renamed_head) {
      ctx->term_pool_offset = term_save;
      return false;
    }

    debug(ctx, "\n--- Trying clause %d ---\n", i);
    debug(ctx, "CLAUSE HEAD (renamed): ");
    debug_term_raw(ctx, renamed_head);
    debug(ctx, "\n");

    if (unify(ctx, selected_goal, renamed_head, env)) {
      debug(ctx, ">>> UNIFIED! Building resolvent...\n");

      int n = c->body_count + cn->count - 1;
      *resolvent = goals_alloc(ctx, n > 0 ? n : 0);
      if (n > 0 && !resolvent->goals)
        return false;

      for (int j = 0; j < c->body_count; j++) {
        term_t *rg = rename_vars_mapped(ctx, c->body[j], &map);
        if (!rg)
          return false;
        resolvent->goals[resolvent->count++] = rg;
      }

      for (int j = 1; j < cn->count; j++)
        resolvent->goals[resolvent->count++] = cn->goals[j];

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
    ctx->term_pool_offset = term_save;
    debug(ctx, "--- Clause %d failed ---\n", i);
  }

  debug(ctx, "=== SON: no match found ===\n");
  env->count = ctx->bind_count = env_mark;
  return false;
}

//****
//* solver main loop
//****

// deref, but only following bindings created before env index `limit`.
// used to see a goal argument's value as it was *before* the clause match
// we're now checking alternatives for — that match may have just bound the
// same variable (e.g. unifying p(X) with p(a) binds X=a), and that binding
// must not make later, still-viable clauses look excluded by indexing.
static term_t *deref_before(env_t *env, term_t *t, int limit) {
  while (t && t->type == VAR) {
    term_t *val = NULL;
    for (int i = limit - 1; i >= 0; i--) {
      if (env->bindings[i].var_id == t->arity) {
        val = env->bindings[i].value;
        break;
      }
    }
    if (!val)
      break;
    t = val;
  }
  return t;
}

static bool has_more_alternatives(trilog_ctx_t *ctx, term_t *goal, env_t *env,
                                  int from_clause, int env_mark) {
  if (from_clause < 0)
    return false; // builtin match
  goal = deref_before(env, goal, env_mark);
  int goal_arity = (goal->type == FUNC) ? goal->arity : 0;
  // first-argument indexing: if the goal's first arg was already ground
  // before this clause match (a literal, or a variable bound earlier in the
  // query), skip clauses whose first arg is a different ground term.
  term_t *goal_a0 = NULL;
  if (goal_arity > 0) {
    term_t *a0 = deref_before(env, goal->args[0], env_mark);
    if (a0->type != VAR)
      goal_a0 = a0;
  }
  for (int i = from_clause; i < ctx->db_count; i++) {
    clause_t *c = &ctx->database[i];
    int head_arity = (c->head->type == FUNC) ? c->head->arity : 0;
    if (goal->name == c->head->name && goal_arity == head_arity) {
      if (goal_a0 && head_arity > 0) {
        term_t *ha0 = c->head->args[0];
        if (ha0->type != VAR &&
            !(ha0->type == goal_a0->type && ha0->name == goal_a0->name) &&
            !(is_cons(goal_a0) && is_cons(ha0)) &&
            !(is_nil(goal_a0) && is_nil(ha0)))
          continue;
      }
      return true;
    }
  }
  return false;
}

bool solve_all(trilog_ctx_t *ctx, goal_stmt_t *initial_goals, env_t *env,
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
  stack[sp].cut_point = 1;
  stack[sp].term_mark = ctx->term_pool_offset;
  sp++;
  if (sp > ctx->stats.stack_peak)
    ctx->stats.stack_peak = sp;

  int clause_idx;
  int env_mark;
  // stack[0] is a sentinel frame the backtrack label (C) never actually
  // resumes into (sp<=0 there means "exhausted"), so 1 is the lowest cut
  // target that leaves it intact. a cut firing before any real choice point
  // has been pushed must not prune sp below the sentinel.
  int cut_point = 1;
  bool found_any = false;

A:
  // yield callback: fire every n steps so embedders can inspect stats
  if (ctx->solve_yield_cb &&
      ++ctx->solve_step_counter >= ctx->solve_yield_interval) {
    ctx->solve_step_counter = 0;
    if (!ctx->solve_yield_cb(ctx, sp, ctx->solve_yield_ud))
      return found_any;
  }

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
      if (!callback(ctx, env, userdata, sp > 1)) {
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
    sp = cut_point;
    int ncut = cn.count - 1;
    goal_stmt_t new_cn = goals_alloc(ctx, ncut > 0 ? ncut : 0);
    if (ncut > 0 && !new_cn.goals)
      return false;
    for (int i = 1; i < cn.count; i++)
      new_cn.goals[new_cn.count++] = cn.goals[i];
    cn = new_cn;
    goto A;
  }

  // inline call/N (N >= 1): call(G, A1, ..., An) -> G(A1, ..., An)
  if (first_goal->type == FUNC && strcmp(first_goal->name, "call") == 0 &&
      first_goal->arity >= 1) {
    term_t *g = deref(env, first_goal->args[0]);
    int extra = first_goal->arity - 1;
    if (g->type == VAR) {
      throw_instantiation_error(ctx, "call/N");
      return false;
    }
    if (g->type != FUNC && g->type != CONST) {
      throw_type_error(ctx, "callable", g, "call/N");
      return false;
    }
    term_t *new_goal;
    if (extra == 0) {
      new_goal = g;
    } else {
      int base_arity = (g->type == FUNC) ? g->arity : 0;
      int new_arity = base_arity + extra;
      term_t *args[16];
      for (int i = 0; i < base_arity; i++)
        args[i] = g->args[i];
      for (int i = 0; i < extra; i++)
        args[base_arity + i] = deref(env, first_goal->args[1 + i]);
      new_goal = make_func(ctx, g->name, args, new_arity);
      if (!new_goal)
        return false;
    }
    goal_stmt_t new_cn = goals_alloc(ctx, cn.count);
    if (!new_cn.goals)
      return false;
    new_cn.goals[new_cn.count++] = new_goal;
    for (int i = 1; i < cn.count; i++)
      new_cn.goals[new_cn.count++] = cn.goals[i];
    cn = new_cn;
    goto A;
  }

  // inline ,/2 (conjunction): ','(a,b) -> a, b
  if (first_goal->type == FUNC && strcmp(first_goal->name, ",") == 0 &&
      first_goal->arity == 2) {
    term_t *left = deref(env, first_goal->args[0]);
    term_t *right = deref(env, first_goal->args[1]);
    goal_stmt_t new_cn = goals_alloc(ctx, cn.count + 1);
    if (!new_cn.goals)
      return false;
    new_cn.goals[new_cn.count++] = left;
    new_cn.goals[new_cn.count++] = right;
    for (int i = 1; i < cn.count; i++)
      new_cn.goals[new_cn.count++] = cn.goals[i];
    cn = new_cn;
    goto A;
  }

  // inline catch/3: catch(goal, catcher, recovery)
  if (first_goal->type == FUNC && strcmp(first_goal->name, "catch") == 0 &&
      first_goal->arity == 3) {
    term_t *sub_goal = deref(env, first_goal->args[0]);
    term_t *catcher = first_goal->args[1];
    term_t *recovery = deref(env, first_goal->args[2]);
    int emark = env->count;

    goal_stmt_t sub_goals = goals_alloc(ctx, 1);
    if (!sub_goals.goals)
      return false;
    sub_goals.goals[sub_goals.count++] = sub_goal;

    int bfloor_save = ctx->bind_floor;
    if (emark > ctx->bind_floor)
      ctx->bind_floor = emark;
    bool sub_ok = solve(ctx, &sub_goals, env);
    ctx->bind_floor = bfloor_save;

    if (sub_ok) {
      // goal succeeded — continue with remaining goals
      int nrem = cn.count - 1;
      goal_stmt_t new_cn = goals_alloc(ctx, nrem > 0 ? nrem : 0);
      if (nrem > 0 && !new_cn.goals)
        return false;
      for (int i = 1; i < cn.count; i++)
        new_cn.goals[new_cn.count++] = cn.goals[i];
      cn = new_cn;
      goto A;
    }

    if (ctx->thrown_ball) {
      // exception was thrown — try to catch it
      term_t *ball = ctx->thrown_ball;
      ctx->thrown_ball = NULL;
      ctx->has_runtime_error = false;
      env->count = ctx->bind_count = emark;

      if (unify(ctx, catcher, ball, env)) {
        // caught - execute recovery
        goal_stmt_t new_cn = goals_alloc(ctx, cn.count);
        if (!new_cn.goals)
          return false;
        new_cn.goals[new_cn.count++] = recovery;
        for (int i = 1; i < cn.count; i++)
          new_cn.goals[new_cn.count++] = cn.goals[i];
        cn = new_cn;
        goto A;
      }
      // catcher didn't match - re-throw
      ctx->thrown_ball = ball;
      ctx->has_runtime_error = true;
      env->count = ctx->bind_count = emark;
      return false;
    }

    // goal failed normally (no exception)
    env->count = ctx->bind_count = emark;
    goto C;
  }

  // inline ;/2 (disjunction / if-then-else)
  if (first_goal->type == FUNC && strcmp(first_goal->name, ";") == 0 &&
      first_goal->arity == 2) {
    term_t *left = deref(env, first_goal->args[0]);
    term_t *right = deref(env, first_goal->args[1]);

    if (left->type == FUNC && strcmp(left->name, "->") == 0 &&
        left->arity == 2) {
      // if-then-else: ;(->(cond, then), else)
      term_t *cond = deref(env, left->args[0]);
      term_t *then_branch = deref(env, left->args[1]);
      int emark = env->count;

      goal_stmt_t cond_goals = goals_alloc(ctx, 1);
      if (!cond_goals.goals)
        return false;
      cond_goals.goals[cond_goals.count++] = cond;

      int bfloor_save = ctx->bind_floor;
      if (emark > ctx->bind_floor)
        ctx->bind_floor = emark;
      bool cond_ok = solve(ctx, &cond_goals, env);
      ctx->bind_floor = bfloor_save;

      if (cond_ok) {
        // cond succeeded — commit to then branch
        goal_stmt_t new_cn = goals_alloc(ctx, cn.count);
        if (!new_cn.goals)
          return false;
        new_cn.goals[new_cn.count++] = then_branch;
        for (int i = 1; i < cn.count; i++)
          new_cn.goals[new_cn.count++] = cn.goals[i];
        cn = new_cn;
      } else if (ctx->thrown_ball) {
        // cond threw — propagate, don't take else branch
        return false;
      } else {
        // cond failed — take else branch
        env->count = ctx->bind_count = emark;
        goal_stmt_t new_cn = goals_alloc(ctx, cn.count);
        if (!new_cn.goals)
          return false;
        new_cn.goals[new_cn.count++] = right;
        for (int i = 1; i < cn.count; i++)
          new_cn.goals[new_cn.count++] = cn.goals[i];
        cn = new_cn;
      }
      goto A;
    }

    // plain disjunction: ;(a, b)
    // push choice point for b, then try a
    {
      goal_stmt_t alt_cn = goals_alloc(ctx, cn.count);
      if (!alt_cn.goals)
        return false;
      alt_cn.goals[alt_cn.count++] = right;
      for (int i = 1; i < cn.count; i++)
        alt_cn.goals[alt_cn.count++] = cn.goals[i];

      assert(sp < MAX_STACK && "Stack overflow");
      stack[sp].goals = alt_cn;
      stack[sp].clause_index = 0;
      stack[sp].env_mark = env->count;
      stack[sp].cut_point = cut_point;
      stack[sp].term_mark = ctx->term_pool_offset;
      sp++;
      if (sp > ctx->stats.stack_peak)
        ctx->stats.stack_peak = sp;

      goal_stmt_t new_cn = goals_alloc(ctx, cn.count);
      if (!new_cn.goals)
        return false;
      new_cn.goals[new_cn.count++] = left;
      for (int i = 1; i < cn.count; i++)
        new_cn.goals[new_cn.count++] = cn.goals[i];
      cn = new_cn;
      goto A;
    }
  }

  // inline ->/2 (standalone if-then, no else — fails if cond fails)
  if (first_goal->type == FUNC && strcmp(first_goal->name, "->") == 0 &&
      first_goal->arity == 2) {
    term_t *cond = deref(env, first_goal->args[0]);
    term_t *then_branch = deref(env, first_goal->args[1]);
    int emark = env->count;

    goal_stmt_t cond_goals = goals_alloc(ctx, 1);
    if (!cond_goals.goals)
      return false;
    cond_goals.goals[cond_goals.count++] = cond;

    int bfloor_save = ctx->bind_floor;
    if (emark > ctx->bind_floor)
      ctx->bind_floor = emark;
    bool cond_ok = solve(ctx, &cond_goals, env);
    ctx->bind_floor = bfloor_save;

    if (cond_ok) {
      goal_stmt_t new_cn = goals_alloc(ctx, cn.count);
      if (!new_cn.goals)
        return false;
      new_cn.goals[new_cn.count++] = then_branch;
      for (int i = 1; i < cn.count; i++)
        new_cn.goals[new_cn.count++] = cn.goals[i];
      cn = new_cn;
      goto A;
    } else if (ctx->thrown_ball) {
      return false;
    } else {
      env->count = ctx->bind_count = emark;
      goto C;
    }
  }

  clause_idx = 0;
  env_mark = env->count;

B:
  debug(ctx, "\n*** LABEL B: trying son, clause_idx=%d, env_mark=%d ***\n",
        clause_idx, env_mark);
  {
    int term_mark_b = ctx->term_pool_offset;
    goal_stmt_t resolvent;
    if (son(ctx, &cn, &clause_idx, env, env_mark, &resolvent)) {
      assert(sp < MAX_STACK && "Stack overflow");
      debug(ctx, "*** SON succeeded, pushing frame, sp=%d ***\n", sp);

      if (has_more_alternatives(ctx, cn.goals[0], env, clause_idx, env_mark)) {
        assert(sp < MAX_STACK && "Stack overflow");
        stack[sp].goals = cn;
        stack[sp].clause_index = clause_idx;
        stack[sp].env_mark = env_mark;
        stack[sp].cut_point = cut_point;
        stack[sp].term_mark = term_mark_b;
        sp++;
        if (sp > ctx->stats.stack_peak)
          ctx->stats.stack_peak = sp;
        cut_point = sp - 1;
      } else if (clause_idx >= 0 && ctx->bind_count > env_mark &&
                 resolvent.count > 0 && env_mark > ctx->bind_floor &&
                 lco_safe(env, env_mark, ctx->bind_count)) {
        // lco: no more alternatives — substitute bindings into resolvent
        // and reclaim binding slots from this deterministic clause.
        // also patch existing binding values that reference vars in the
        // reclaimed range, so the binding chain doesn't break.
        for (int j = 0; j < resolvent.count; j++)
          resolvent.goals[j] = substitute(ctx, env, resolvent.goals[j]);
        // patched binding values must survive backtracking.  record the
        // offset before patching so we can ratchet the enclosing choice
        // point's term_mark only if new terms were actually allocated.
        int pre_patch = ctx->term_pool_offset;
        for (int j = 0; j < env_mark; j++) {
          if (!term_refs_range(env, env->bindings[j].value, env_mark,
                               ctx->bind_count))
            continue;
          env->bindings[j].value = substitute(ctx, env, env->bindings[j].value);
        }
        if (ctx->term_pool_offset > pre_patch && sp > 1)
          stack[sp - 1].term_mark = ctx->term_pool_offset;
        env->count = ctx->bind_count = env_mark;
      } else if (clause_idx >= 0 && ctx->bind_count == env_mark &&
                 resolvent.count > 0 && sp <= 1 && cn.count == 1) {
        // deterministic tail call with no new bindings, no choice points.
        // the matched goal was the sole remaining goal (tail position).
        // reset temp pool to the initial level and rebuild the resolvent
        // fresh from the perm-pool clause body.
        clause_t *c = &ctx->database[clause_idx - 1];
        int base_term = stack[0].term_mark;
        if (base_term < ctx->term_pool_floor)
          base_term = ctx->term_pool_floor;
        int base_env = stack[0].env_mark;
        if (base_env < ctx->bind_floor)
          base_env = ctx->bind_floor;
        env->count = ctx->bind_count = base_env;
        ctx->term_pool_offset = base_term;
        var_id_map_t map = {0};
        resolvent = goals_alloc(ctx, c->body_count);
        if (c->body_count > 0 && !resolvent.goals)
          return false;
        for (int j = 0; j < c->body_count; j++)
          resolvent.goals[resolvent.count++] =
              rename_vars_mapped(ctx, c->body[j], &map);
      }

      cn = resolvent;
      goto A;
    } else {
      debug(ctx, "*** SON failed, going to C ***\n");
      if (ctx->has_runtime_error)
        return false;
      goto C;
    }
  }

C:
  if (ctx->has_runtime_error)
    return false;
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
  {
    int restored = stack[sp].term_mark;
    if (restored < ctx->term_pool_floor)
      restored = ctx->term_pool_floor;
    ctx->term_pool_offset = restored;
  }

  assert(env_mark >= 0 && env_mark <= env->count &&
         "Invalid env_mark from stack");
  env->count = ctx->bind_count = env_mark;

  debug(ctx, "*** Restored: clause_idx=%d, env_mark=%d, cut_point=%d ***\n",
        clause_idx, env_mark, cut_point);
  // clause_idx == 0 means this was a disjunction choice point (not a clause
  // retry), so go to a to allow inline handlers (,/2, ->/2, etc.) to run.
  if (clause_idx == 0)
    goto A;
  goto B;
}

bool solve(trilog_ctx_t *ctx, goal_stmt_t *initial_goals, env_t *env) {
  return solve_all(ctx, initial_goals, env, NULL, NULL);
}