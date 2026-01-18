#pragma once

#include <stdbool.h>
#include <stdio.h>

#define MAX_NAME 64
#define MAX_ARGS 8
#define MAX_CLAUSES 256
#define MAX_BINDINGS 256
#define MAX_GOALS 64
#define MAX_STACK 256
#define MAX_TERMS 4096
#define MAX_ERROR_MSG 256

typedef enum { CONST, VAR, FUNC } term_type;

typedef struct term {
  term_type type;
  char name[MAX_NAME];
  struct term *args[MAX_ARGS];
  int arity;
} term_t;

typedef struct {
  char name[MAX_NAME];
  term_t *value;
} binding_t;

typedef struct {
  binding_t bindings[MAX_BINDINGS];
  int count;
} env_t;

typedef struct {
  term_t *head;
  term_t *body[MAX_GOALS];
  int body_count;
} clause_t;

typedef struct {
  term_t *goals[MAX_GOALS];
  int count;
} goal_stmt_t;

typedef struct {
  goal_stmt_t goals;
  int clause_index;
  int env_mark;
} frame_t;

typedef struct {
  bool has_error;
  char message[MAX_ERROR_MSG];
  int line;
  int column;
} parse_error_t;

typedef struct {
  clause_t database[MAX_CLAUSES];
  int db_count;
  int var_counter;
  char *input_ptr;
  char *input_start;
  int input_line;
  bool debug_enabled;

  term_t term_pool[MAX_TERMS];
  int term_count;

  parse_error_t error;
} prolog_ctx_t;

void ctx_reset_terms(prolog_ctx_t *ctx);
term_t *ctx_alloc_term(prolog_ctx_t *ctx);

void debug(prolog_ctx_t *ctx, const char *fmt, ...);
void print_term_raw(term_t *t);
void debug_term_raw(prolog_ctx_t *ctx, term_t *t);

term_t *make_term(prolog_ctx_t *ctx, term_type type, const char *name,
                  term_t **args, int arity);
term_t *make_const(prolog_ctx_t *ctx, const char *name);
term_t *make_var(prolog_ctx_t *ctx, const char *name);
term_t *make_func(prolog_ctx_t *ctx, const char *name, term_t **args,
                  int arity);

void skip_ws(prolog_ctx_t *ctx);
term_t *parse_term(prolog_ctx_t *ctx);
term_t *parse_list(prolog_ctx_t *ctx);
void parse_clause(prolog_ctx_t *ctx, char *line);

term_t *lookup(env_t *env, const char *name);
void bind(prolog_ctx_t *ctx, env_t *env, const char *name, term_t *value);
term_t *deref(env_t *env, term_t *t);
term_t *substitute(prolog_ctx_t *ctx, env_t *env, term_t *t);

bool unify(prolog_ctx_t *ctx, term_t *a, term_t *b, env_t *env);

term_t *rename_vars(prolog_ctx_t *ctx, term_t *t, int id);

bool son(prolog_ctx_t *ctx, goal_stmt_t *cn, int *clause_idx, env_t *env,
         int env_mark, goal_stmt_t *resolvent);
bool solve(prolog_ctx_t *ctx, goal_stmt_t *initial_goals, env_t *env);

void print_term(term_t *t, env_t *env);

void parse_error(prolog_ctx_t *ctx, const char *fmt, ...);
void parse_error_clear(prolog_ctx_t *ctx);
bool parse_has_error(prolog_ctx_t *ctx);
void parse_error_print(prolog_ctx_t *ctx);

int try_builtin(prolog_ctx_t *ctx, term_t *goal, env_t *env);