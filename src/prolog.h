#pragma once

// freestanding mode: user must provide these macros before including this
// header hosted mode: we use stdlib normally
#ifndef PROLOG_FREESTANDING

#include <stdarg.h>
#include <stdbool.h>

#else // PROLOG_FREESTANDING

#ifndef bool
typedef _Bool bool;
#define true 1
#define false 0
#endif

#ifndef _VA_LIST_DEFINED
typedef __builtin_va_list va_list;
#define va_start __builtin_va_start
#define va_end __builtin_va_end
#define va_arg __builtin_va_arg
#define _VA_LIST_DEFINED
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

// user must define these as macros pointing to their implementations:
// strcmp, strncmp, strlen, strchr, strcspn, strncpy
// isspace, isdigit, isalpha, isalnum
// vsnprintf, snprintf
// assert (or define NDEBUG to disable)

#endif // PROLOG_FREESTANDING

#define MAX_NAME 64
#define MAX_ARGS 8
#define MAX_CLAUSES 256
#define MAX_BINDINGS 256
#define MAX_GOALS 64
#define MAX_STACK 256
#define MAX_TERMS 4096
#define MAX_ERROR_MSG 256
#define MAX_CUSTOM_BUILTINS 64
#define MAX_STRING_POOL 65536

typedef struct prolog_ctx prolog_ctx_t;
typedef struct term term_t;
typedef struct env env_t;

typedef int (*builtin_handler_t)(prolog_ctx_t *ctx, term_t *goal, env_t *env);

typedef struct {
  char name[MAX_NAME];
  int arity;
  builtin_handler_t handler;
  void *userdata;
} custom_builtin_t;

typedef void (*io_write_callback_t)(prolog_ctx_t *ctx, const char *str,
                                    void *userdata);
typedef void (*io_write_term_callback_t)(prolog_ctx_t *ctx, term_t *t,
                                         env_t *env, void *userdata);
typedef void (*io_writef_callback_t)(prolog_ctx_t *ctx, const char *fmt,
                                     va_list args, void *userdata);
typedef int (*io_read_char_callback_t)(prolog_ctx_t *ctx, void *userdata);
typedef char *(*io_read_line_callback_t)(prolog_ctx_t *ctx, char *buf, int size,
                                         void *userdata);

typedef struct {
  io_write_callback_t write_str;
  io_write_term_callback_t write_term;
  io_writef_callback_t writef;
  io_read_char_callback_t read_char;
  io_read_line_callback_t read_line;
  void *userdata;
} io_hooks_t;

typedef enum { CONST, VAR, FUNC, STRING } term_type;

struct term {
  term_type type;
  char name[MAX_NAME];
  struct term *args[MAX_ARGS];
  int arity;
  char *string_data;
};

typedef struct {
  char name[MAX_NAME];
  term_t *value;
} binding_t;

struct env {
  binding_t bindings[MAX_BINDINGS];
  int count;
};

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
  int cut_point; // stack pointer to cut back to
} frame_t;

typedef struct {
  bool has_error;
  char message[MAX_ERROR_MSG];
  int line;
  int column;
} parse_error_t;

struct prolog_ctx {
  clause_t database[MAX_CLAUSES];
  int db_count;
  int var_counter;
  char *input_ptr;
  char *input_start;
  int input_line;
  bool debug_enabled;

  term_t term_pool[MAX_TERMS];
  int term_count;

  char string_pool[MAX_STRING_POOL];
  int string_pool_offset;

  parse_error_t error;

  io_hooks_t io_hooks;

  custom_builtin_t custom_builtins[MAX_CUSTOM_BUILTINS];
  int custom_builtin_count;

  struct {
    int terms_allocated;
    int terms_peak;
    int unify_calls;
    int unify_fails;
    int son_calls;
    int backtracks;
  } stats;
};

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
term_t *make_string(prolog_ctx_t *ctx, const char *str);

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

typedef bool (*solution_callback_t)(prolog_ctx_t *ctx, env_t *env,
                                    void *userdata);

bool solve(prolog_ctx_t *ctx, goal_stmt_t *initial_goals, env_t *env);
bool solve_all(prolog_ctx_t *ctx, goal_stmt_t *initial_goals, env_t *env,
               solution_callback_t callback, void *userdata);

void print_term(prolog_ctx_t *ctx, term_t *t, env_t *env);

void parse_error(prolog_ctx_t *ctx, const char *fmt, ...);
void parse_error_clear(prolog_ctx_t *ctx);
bool parse_has_error(prolog_ctx_t *ctx);
void parse_error_print(prolog_ctx_t *ctx);

int try_builtin(prolog_ctx_t *ctx, term_t *goal, env_t *env);

// I/O hook management
void io_hooks_init_default(prolog_ctx_t *ctx);
void io_hooks_set(prolog_ctx_t *ctx, io_hooks_t *hooks);

// I/O functions (use hooks internally)
void io_write_str(prolog_ctx_t *ctx, const char *str);
void io_write_term(prolog_ctx_t *ctx, term_t *t, env_t *env);
void io_writef(prolog_ctx_t *ctx, const char *fmt, ...);
int io_read_char(prolog_ctx_t *ctx);
char *io_read_line(prolog_ctx_t *ctx, char *buf, int size);

// FFI: Register custom builtins
bool ffi_register_builtin(prolog_ctx_t *ctx, const char *name, int arity,
                          builtin_handler_t handler, void *userdata);
void ffi_clear_builtins(prolog_ctx_t *ctx);
custom_builtin_t *ffi_get_builtin_userdata(prolog_ctx_t *ctx, term_t *goal);
