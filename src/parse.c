#include "prolog.h"
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void parse_error(prolog_ctx_t *ctx, const char *fmt, ...) {
  if (ctx->error.has_error)
    return;

  ctx->error.has_error = true;
  ctx->error.line = ctx->input_line;
  ctx->error.column = (int)(ctx->input_ptr - ctx->input_start) + 1;

  va_list args;
  va_start(args, fmt);
  vsnprintf(ctx->error.message, MAX_ERROR_MSG, fmt, args);
  va_end(args);
}

void parse_error_clear(prolog_ctx_t *ctx) {
  ctx->error.has_error = false;
  ctx->error.message[0] = '\0';
  ctx->error.line = 0;
  ctx->error.column = 0;
}

bool parse_has_error(prolog_ctx_t *ctx) { return ctx->error.has_error; }

void parse_error_print(prolog_ctx_t *ctx) {
  if (!ctx->error.has_error)
    return;
  fprintf(stderr, "Parse error at line %d, column %d: %s\n", ctx->error.line,
          ctx->error.column, ctx->error.message);
}

void skip_ws(prolog_ctx_t *ctx) {
  assert(ctx != NULL && "Context is NULL");
  assert(ctx->input_ptr != NULL && "Input pointer is NULL");
  while (*ctx->input_ptr && isspace(*ctx->input_ptr))
    ctx->input_ptr++;
}

static term_t *parse_primary(prolog_ctx_t *ctx);
static term_t *parse_infix(prolog_ctx_t *ctx, term_t *left, int min_prec);

// operator precedence (higher = binds tighter)
static int get_precedence(const char *op) {
  if (strcmp(op, "*") == 0 || strcmp(op, "/") == 0 || strcmp(op, "mod") == 0)
    return 40;
  if (strcmp(op, "+") == 0 || strcmp(op, "-") == 0)
    return 30;
  if (strcmp(op, "<") == 0 || strcmp(op, ">") == 0 || strcmp(op, "=<") == 0 ||
      strcmp(op, ">=") == 0 || strcmp(op, "=:=") == 0 ||
      strcmp(op, "=\\=") == 0)
    return 20;
  if (strcmp(op, "is") == 0)
    return 10;
  if (strcmp(op, "=") == 0 || strcmp(op, "\\=") == 0)
    return 10;
  return 0;
}

// len = 0 is a failed parse
static int try_parse_op(prolog_ctx_t *ctx, char *op_out, int max_len) {
  char *p = ctx->input_ptr;

  // three-char operators
  if (p[0] == '=' && p[1] == ':' && p[2] == '=') {
    strncpy(op_out, "=:=", max_len);
    return 3;
  }
  if (p[0] == '=' && p[1] == '\\' && p[2] == '=') {
    strncpy(op_out, "=\\=", max_len);
    return 3;
  }
  if (p[0] == '\\' && p[1] == '=' && p[2] != '=') {
    strncpy(op_out, "\\=", max_len);
    return 2;
  }

  // two-char operators
  if (p[0] == '=' && p[1] == '<') {
    strncpy(op_out, "=<", max_len);
    return 2;
  }
  if (p[0] == '>' && p[1] == '=') {
    strncpy(op_out, ">=", max_len);
    return 2;
  }

  // keyword operators
  if (strncmp(p, "is", 2) == 0 && !isalnum(p[2]) && p[2] != '_') {
    strncpy(op_out, "is", max_len);
    return 2;
  }
  if (strncmp(p, "mod", 3) == 0 && !isalnum(p[3]) && p[3] != '_') {
    strncpy(op_out, "mod", max_len);
    return 3;
  }

  // single-char operators
  if (p[0] == '+' || p[0] == '*' || p[0] == '/' || p[0] == '<' || p[0] == '>' ||
      p[0] == '=') {
    op_out[0] = p[0];
    op_out[1] = '\0';
    return 1;
  }

  if (p[0] == '-' && !isdigit(p[1])) {
    op_out[0] = '-';
    op_out[1] = '\0';
    return 1;
  }

  return 0;
}

term_t *parse_list(prolog_ctx_t *ctx) {
  assert(ctx != NULL && "Context is NULL");
  assert(ctx->input_ptr != NULL && "Input pointer is NULL");

  if (*ctx->input_ptr != '[') {
    parse_error(ctx, "expected '[' at start of list");
    return NULL;
  }

  ctx->input_ptr++;
  skip_ws(ctx);

  if (*ctx->input_ptr == ']') {
    ctx->input_ptr++;
    return make_const(ctx, "[]");
  }

  term_t *elements[MAX_ARGS];
  int count = 0;
  term_t *tail = NULL;

  elements[count] = parse_term(ctx);
  if (!elements[count]) {
    parse_error(ctx, "failed to parse list element");
    return NULL;
  }
  count++;
  skip_ws(ctx);

  while (*ctx->input_ptr == ',' || *ctx->input_ptr == '|') {
    if (*ctx->input_ptr == '|') {
      ctx->input_ptr++;
      skip_ws(ctx);
      tail = parse_term(ctx);
      if (!tail) {
        parse_error(ctx, "failed to parse list tail after '|'");
        return NULL;
      }
      skip_ws(ctx);
      break;
    }
    ctx->input_ptr++;
    skip_ws(ctx);

    if (count >= MAX_ARGS) {
      parse_error(ctx, "too many list elements (max %d)", MAX_ARGS);
      return NULL;
    }

    elements[count] = parse_term(ctx);
    if (!elements[count]) {
      parse_error(ctx, "failed to parse list element");
      return NULL;
    }
    count++;
    skip_ws(ctx);
  }

  if (*ctx->input_ptr != ']') {
    parse_error(ctx, "expected ']' to close list, got '%c'",
                *ctx->input_ptr ? *ctx->input_ptr : '?');
    return NULL;
  }
  ctx->input_ptr++;

  term_t *result = tail ? tail : make_const(ctx, "[]");
  for (int i = count - 1; i >= 0; i--) {
    term_t *args[2] = {elements[i], result};
    result = make_func(ctx, ".", args, 2);
  }

  return result;
}

static term_t *parse_primary(prolog_ctx_t *ctx) {
  assert(ctx != NULL && "Context is NULL");
  assert(ctx->input_ptr != NULL && "Input pointer is NULL");

  if (parse_has_error(ctx))
    return NULL;

  skip_ws(ctx);

  debug(ctx, "DEBUG parse_primary: next char = '%c'\n",
        *ctx->input_ptr ? *ctx->input_ptr : '?');

  if (*ctx->input_ptr == '\0') {
    return NULL; // end of input, not necessarily an error
  }

  // parenthesized expression
  if (*ctx->input_ptr == '(') {
    ctx->input_ptr++;
    skip_ws(ctx);
    term_t *inner = parse_term(ctx);
    if (!inner) {
      parse_error(ctx, "expected expression inside parentheses");
      return NULL;
    }
    skip_ws(ctx);
    if (*ctx->input_ptr != ')') {
      parse_error(ctx, "expected ')' after expression, got '%c'",
                  *ctx->input_ptr ? *ctx->input_ptr : '?');
      return NULL;
    }
    ctx->input_ptr++;
    return inner;
  }

  if (*ctx->input_ptr == '[')
    return parse_list(ctx);

  char name[MAX_NAME] = {0};
  int i = 0;

  if (*ctx->input_ptr == '!') {
    name[i++] = *ctx->input_ptr++;
  } else if (isdigit(*ctx->input_ptr) ||
             (*ctx->input_ptr == '-' && isdigit(ctx->input_ptr[1]))) {
    if (*ctx->input_ptr == '-')
      name[i++] = *ctx->input_ptr++;
    while (isdigit(*ctx->input_ptr)) {
      if (i >= MAX_NAME - 1) {
        parse_error(ctx, "number too long (max %d digits)", MAX_NAME - 1);
        return NULL;
      }
      name[i++] = *ctx->input_ptr++;
    }
  } else if (isalpha(*ctx->input_ptr) || *ctx->input_ptr == '_') {
    while (isalnum(*ctx->input_ptr) || *ctx->input_ptr == '_') {
      if (i >= MAX_NAME - 1) {
        parse_error(ctx, "name too long (max %d chars)", MAX_NAME - 1);
        return NULL;
      }
      name[i++] = *ctx->input_ptr++;
    }
  } else {
    // Not a valid start of term
    return NULL;
  }

  debug(ctx, "DEBUG parse_primary: parsed name = '%s'\n", name);

  skip_ws(ctx);

  if (*ctx->input_ptr == '(') {
    ctx->input_ptr++;
    term_t *args[MAX_ARGS];
    int arity = 0;

    skip_ws(ctx);
    if (*ctx->input_ptr != ')') {
      do {
        skip_ws(ctx);
        if (arity >= MAX_ARGS) {
          parse_error(ctx, "too many arguments (max %d)", MAX_ARGS);
          return NULL;
        }
        args[arity] = parse_term(ctx);
        if (!args[arity]) {
          if (!parse_has_error(ctx)) {
            parse_error(ctx, "failed to parse argument %d", arity + 1);
          }
          return NULL;
        }
        arity++;
        skip_ws(ctx);
      } while (*ctx->input_ptr == ',' && ctx->input_ptr++);
    }
    skip_ws(ctx);

    if (*ctx->input_ptr != ')') {
      parse_error(ctx, "expected ')' after arguments, got '%c'",
                  *ctx->input_ptr ? *ctx->input_ptr : '?');
      return NULL;
    }
    ctx->input_ptr++;

    debug(ctx, "DEBUG parse_primary: functor %s/%d\n", name, arity);
    return make_func(ctx, name, args, arity);
  }

  if (isupper(name[0]) || name[0] == '_') {
    debug(ctx, "DEBUG parse_primary: variable %s\n", name);
    return make_var(ctx, name);
  }
  debug(ctx, "DEBUG parse_primary: constant %s\n", name);
  return make_const(ctx, name);
}

static term_t *parse_infix(prolog_ctx_t *ctx, term_t *left, int min_prec) {
  while (1) {
    skip_ws(ctx);

    char op[8] = {0};
    int op_len = try_parse_op(ctx, op, sizeof(op));

    if (op_len == 0)
      return left;

    int prec = get_precedence(op);
    if (prec < min_prec)
      return left;

    ctx->input_ptr += op_len;
    skip_ws(ctx);

    term_t *right = parse_primary(ctx);
    if (!right) {
      parse_error(ctx, "expected term after '%s'", op);
      return NULL;
    }

    // Look ahead for higher precedence operator
    skip_ws(ctx);
    char next_op[8] = {0};
    int next_len = try_parse_op(ctx, next_op, sizeof(next_op));

    while (next_len > 0 && get_precedence(next_op) > prec) {
      right = parse_infix(ctx, right, get_precedence(next_op));
      if (!right)
        return NULL;
      skip_ws(ctx);
      next_len = try_parse_op(ctx, next_op, sizeof(next_op));
    }

    term_t *args[2] = {left, right};
    left = make_func(ctx, op, args, 2);
  }
}

term_t *parse_term(prolog_ctx_t *ctx) {
  assert(ctx != NULL && "Context is NULL");
  assert(ctx->input_ptr != NULL && "Input pointer is NULL");

  if (parse_has_error(ctx))
    return NULL;

  term_t *left = parse_primary(ctx);
  if (!left)
    return NULL;

  return parse_infix(ctx, left, 0);
}

void parse_clause(prolog_ctx_t *ctx, char *line) {
  assert(ctx != NULL && "Context is NULL");
  assert(line != NULL && "Line cannot be NULL");

  parse_error_clear(ctx);
  ctx->input_ptr = line;
  ctx->input_start = line;

  if (ctx->db_count >= MAX_CLAUSES) {
    parse_error(ctx, "database full (max %d clauses)", MAX_CLAUSES);
    parse_error_print(ctx);
    return;
  }

  clause_t *c = &ctx->database[ctx->db_count];
  debug(ctx, "=== Parsing clause ===\n");

  c->head = parse_term(ctx);
  if (!c->head) {
    if (!parse_has_error(ctx)) {
      parse_error(ctx, "failed to parse clause head");
    }
    parse_error_print(ctx);
    return;
  }
  c->body_count = 0;

  skip_ws(ctx);
  if (ctx->input_ptr[0] == ':' && ctx->input_ptr[1] == '-') {
    ctx->input_ptr += 2;
    debug(ctx, "=== Parsing body ===\n");
    do {
      skip_ws(ctx);
      term_t *g = parse_term(ctx);
      if (!g) {
        if (parse_has_error(ctx)) {
          parse_error_print(ctx);
          return;
        }
        break;
      }
      if (c->body_count >= MAX_GOALS) {
        parse_error(ctx, "too many goals in clause body (max %d)", MAX_GOALS);
        parse_error_print(ctx);
        return;
      }
      c->body[c->body_count++] = g;
      skip_ws(ctx);
    } while (*ctx->input_ptr == ',' && ctx->input_ptr++);
  }

  ctx->db_count++;

  if (ctx->debug_enabled) {
    debug(ctx, "=== Clause %d parsed ===\n", ctx->db_count - 1);
    debug(ctx, "HEAD: ");
    debug_term_raw(ctx, c->head);
    debug(ctx, "\n");
    for (int i = 0; i < c->body_count; i++) {
      debug(ctx, "BODY[%d]: ", i);
      debug_term_raw(ctx, c->body[i]);
      debug(ctx, "\n");
    }
    debug(ctx, "======================\n");
  }
}