#include "prolog.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void print_term(term_t *t, env_t *env) {
  assert(env != NULL && "Environment is NULL");

  if (!t) {
    printf("NULL");
    return;
  }

  t = deref(env, t);

  if (t->type == FUNC && strcmp(t->name, ".") == 0 && t->arity == 2) {
    printf("[");
    while (t->type == FUNC && strcmp(t->name, ".") == 0) {
      assert(t->arity == 2 && "List node must have arity 2");
      print_term(t->args[0], env);
      t = deref(env, t->args[1]);
      if (t->type == FUNC && strcmp(t->name, ".") == 0)
        printf(", ");
    }
    if (!(t->type == CONST && strcmp(t->name, "[]") == 0)) {
      printf("|");
      print_term(t, env);
    }
    printf("]");
    return;
  }

  if (t->type == CONST && strcmp(t->name, "[]") == 0) {
    printf("[]");
    return;
  }

  printf("%s", t->name);
  if (t->type == FUNC && t->arity > 0) {
    printf("(");
    for (int i = 0; i < t->arity; i++) {
      if (i > 0)
        printf(", ");
      print_term(t->args[i], env);
    }
    printf(")");
  }
}