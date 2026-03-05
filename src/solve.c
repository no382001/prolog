#include "platform_impl.h"

typedef struct {
  solution_callback_t callback;
  void *userdata;
  bool stop;
} solve_state_t;

// c clauses matching name+arity starting from clause index `from`.
static int count_alternatives(prolog_ctx_t *ctx, const char *name, int arity,
                              int from) {
  int n = 0;
  for (int i = from; i < ctx->db_count; i++) {
    clause_t *c = &ctx->database[i];
    int ha = (c->head->type == FUNC) ? c->head->arity : 0;
    if (strcmp(name, c->head->name) == 0 && arity == ha)
      n++;
  }
  return n;
}

static bool solve_r(prolog_ctx_t *ctx, goal_stmt_t cn, env_t *env,
                    solve_state_t *st, bool *cut_raised, bool has_alt_above);

static bool solve_r(prolog_ctx_t *ctx, goal_stmt_t cn, env_t *env,
                    solve_state_t *st, bool *cut_raised, bool has_alt_above) {
  if (st->stop)
    return false;

  if (cn.count == 0) {
    if (st->callback) {
      if (!st->callback(ctx, env, st->userdata, has_alt_above))
        st->stop = true;
    } else {
      st->stop = true; // no callback: commit to first solution, keep bindings
    }
    return true;
  }

  term_t *first = deref(env, cn.goals[0]);

  // cut
  if (first->type == CONST && strcmp(first->name, "!") == 0) {
    goal_stmt_t rest = {0};
    for (int i = 1; i < cn.count; i++)
      rest.goals[rest.count++] = cn.goals[i];
    bool ok = solve_r(ctx, rest, env, st, cut_raised, has_alt_above);
    *cut_raised = true;
    return ok;
  }

  // call/1 — inline unwrap
  if (first->type == FUNC && strcmp(first->name, "call") == 0 &&
      first->arity == 1) {
    goal_stmt_t new_cn = {0};
    new_cn.goals[new_cn.count++] = deref(env, first->args[0]);
    for (int i = 1; i < cn.count; i++)
      new_cn.goals[new_cn.count++] = cn.goals[i];
    return solve_r(ctx, new_cn, env, st, cut_raised, has_alt_above);
  }

  // builtins
  {
    int br = try_builtin(ctx, first, env);
    if (br != 0) {
      if (br == 1) {
        goal_stmt_t rest = {0};
        for (int i = 1; i < cn.count; i++)
          rest.goals[rest.count++] = cn.goals[i];
        return solve_r(ctx, rest, env, st, cut_raised, has_alt_above);
      }
      return false;
    }
  }

  // user-defined clauses
  int first_arity = (first->type == FUNC) ? first->arity : 0;
  int remaining = count_alternatives(ctx, first->name, first_arity, 0);

  bool found = false;
  int tried = 0;

  for (int i = 0; i < ctx->db_count && !st->stop; i++) {
    clause_t *c = &ctx->database[i];
    int ha = (c->head->type == FUNC) ? c->head->arity : 0;
    if (strcmp(first->name, c->head->name) != 0 || first_arity != ha)
      continue;

    tried++;
    remaining--;

    int env_mark = env->count;
    int term_save = ctx->term_count;
    int id = ++ctx->var_counter;
    term_t *renamed_head = rename_vars(ctx, c->head, id);

    debug(ctx, "\n--- Trying clause %d for %s/%d ---\n", i, first->name,
          first_arity);

    if (unify(ctx, first, renamed_head, env)) {
      debug(ctx, ">>> UNIFIED\n");

      goal_stmt_t new_cn = {0};
      for (int j = 0; j < c->body_count; j++)
        new_cn.goals[new_cn.count++] = rename_vars(ctx, c->body[j], id);
      for (int j = 1; j < cn.count; j++)
        new_cn.goals[new_cn.count++] = cn.goals[j];

      // there are more alternatives if remaining > 0 at this level OR above
      bool sub_has_alt = has_alt_above || (remaining > 0);

      bool sub_cut = false;
      bool sub = solve_r(ctx, new_cn, env, st, &sub_cut, sub_has_alt);
      if (sub)
        found = true;

      if (sub_cut || st->stop) {
        // cut or committed result: don't restore anything
        break;
      }

      // backtrack: undo bindings but NOT term_count — callbacks may have
      // stored pointers into the term pool (e.g. findall substitute results)
      env->count = env_mark;
    } else {
      // unification failed: safe to reclaim terms since nothing references them
      env->count = env_mark;
      ctx->term_count = term_save;
    }
  }

  ctx->stats.backtracks++;
  return found;
}

bool solve_all(prolog_ctx_t *ctx, goal_stmt_t *initial_goals, env_t *env,
               solution_callback_t callback, void *userdata) {
  solve_state_t st = {
      .callback = callback, .userdata = userdata, .stop = false};
  bool cut_raised = false;
  return solve_r(ctx, *initial_goals, env, &st, &cut_raised, false);
}

bool solve(prolog_ctx_t *ctx, goal_stmt_t *initial_goals, env_t *env) {
  return solve_all(ctx, initial_goals, env, NULL, NULL);
}
