#include "platform_impl.h"

void print_term(prolog_ctx_t *ctx, term_t *t, env_t *env) {
  assert(env != ((void*)0) && "Environment is NULL");

  if (!t) {
    io_write_str(ctx, "NULL");
    return;
  }

  t = deref(env, t);

  if (t->type == FUNC && strcmp(t->name, ".") == 0 && t->arity == 2) {
    io_write_str(ctx, "[");
    while (t->type == FUNC && strcmp(t->name, ".") == 0) {
      assert(t->arity == 2 && "List node must have arity 2");
      print_term(ctx, t->args[0], env);
      t = deref(env, t->args[1]);
      if (t->type == FUNC && strcmp(t->name, ".") == 0)
        io_write_str(ctx, ", ");
    }
    if (!(t->type == CONST && strcmp(t->name, "[]") == 0)) {
      io_write_str(ctx, "|");
      print_term(ctx, t, env);
    }
    io_write_str(ctx, "]");
    return;
  }

  if (t->type == CONST && strcmp(t->name, "[]") == 0) {
    io_write_str(ctx, "[]");
    return;
  }

  io_write_str(ctx, t->name);
  if (t->type == FUNC && t->arity > 0) {
    io_write_str(ctx, "(");
    for (int i = 0; i < t->arity; i++) {
      if (i > 0)
        io_write_str(ctx, ", ");
      print_term(ctx, t->args[i], env);
    }
    io_write_str(ctx, ")");
  }
}