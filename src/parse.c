#include "platform_impl.h"

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

  if (ctx->input_start && ctx->error.column > 0) {
    // show the offending line and point to the error
    fprintf(stderr, "  %s\n", ctx->input_start);
    fprintf(stderr, "  %*s^\n", ctx->error.column - 1, "");
    fprintf(stderr, "error: %s\n", ctx->error.message);
  } else {
    // fallback for non-interactive / no context
    fprintf(stderr, "error: line %d, column %d: %s\n", ctx->error.line,
            ctx->error.column, ctx->error.message);
  }
}

void skip_ws(prolog_ctx_t *ctx) {
  assert(ctx != NULL && "Context is NULL");
  assert(ctx->input_ptr != NULL && "Input pointer is NULL");
  while (*ctx->input_ptr && isspace(*ctx->input_ptr))
    ctx->input_ptr++;
}

typedef struct {
  const char *text;
  int len;
  bool is_keyword; // needs non-alnum check after
} op_pattern_t;

typedef struct {
  const char *op;
  int precedence;
} op_prec_t;

static term_t *parse_primary(prolog_ctx_t *ctx);
static term_t *parse_infix(prolog_ctx_t *ctx, term_t *left, int min_prec);
static const op_prec_t precedence_table[] = {
    {"*", 40},    {"/", 40},  {"mod", 40}, {"+", 30},   {"-", 30},
    {"<", 20},    {">", 20},  {"=<", 20},  {">=", 20},  {"=:=", 20},
    {"=\\=", 20}, {"is", 10}, {"=", 10},   {"\\=", 10}, {NULL, 0}};

// ordered longest-first to avoid prefix conflicts
static const op_pattern_t op_patterns[] = {
    {"=:=", 3, false}, {"=\\=", 3, false}, {"mod", 3, true}, {"\\=", 2, false},
    {"=<", 2, false},  {">=", 2, false},   {"is", 2, true},  {"+", 1, false},
    {"*", 1, false},   {"/", 1, false},    {"<", 1, false},  {">", 1, false},
    {"=", 1, false},   {"-", 1, false}, // special handling needed
    {NULL, 0, false}};

static int get_precedence(const char *op) {
  for (const op_prec_t *p = precedence_table; p->op; p++) {
    if (strcmp(op, p->op) == 0)
      return p->precedence;
  }
  return 0;
}

static int try_parse_op(prolog_ctx_t *ctx, char *op_out, int max_len) {
  char *p = ctx->input_ptr;

  for (const op_pattern_t *pat = op_patterns; pat->text; pat++) {
    if (strncmp(p, pat->text, pat->len) != 0)
      continue;

    // keyword operators need non-alnum after
    if (pat->is_keyword) {
      char next = p[pat->len];
      if (isalnum(next) || next == '_')
        continue;
    }

    // special case: minus before digit is negative number, not operator
    if (pat->text[0] == '-' && pat->len == 1 && isdigit(p[1]))
      continue;

    strncpy(op_out, pat->text, max_len);
    op_out[max_len - 1] = '\0';
    return pat->len;
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

  if (*ctx->input_ptr == '\"') {
    ctx->input_ptr++;           // skip opening quote
    char str_buf[MAX_NAME * 4]; // allow longer strings
    int i = 0;

    while (*ctx->input_ptr && *ctx->input_ptr != '\"') {
      if (i >= sizeof(str_buf) - 1) {
        parse_error(ctx, "string too long (max %d chars)",
                    (int)sizeof(str_buf) - 1);
        return NULL;
      }

      // escape sequences
      if (*ctx->input_ptr == '\\\\' && ctx->input_ptr[1]) {
        ctx->input_ptr++;
        switch (*ctx->input_ptr) {
        case 'n':
          str_buf[i++] = '\\n';
          break;
        case 't':
          str_buf[i++] = '\\t';
          break;
        case 'r':
          str_buf[i++] = '\\r';
          break;
        case '\\\\':
          str_buf[i++] = '\\\\';
          break;
        case '\"':
          str_buf[i++] = '\"';
          break;
        default:
          str_buf[i++] = *ctx->input_ptr;
          break;
        }
        ctx->input_ptr++;
      } else {
        str_buf[i++] = *ctx->input_ptr++;
      }
    }

    if (*ctx->input_ptr != '\"') {
      parse_error(ctx, "unterminated string literal");
      return NULL;
    }
    ctx->input_ptr++; // skip closing quote
    str_buf[i] = '\0';

    return make_string(ctx, str_buf);
  }

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

static void strip_line_comment(char *line) {
  bool in_string = false;
  for (char *p = line; *p; p++) {
    if (in_string) {
      if (*p == '\\' && *(p + 1))
        p++;
      else if (*p == '"')
        in_string = false;
    } else {
      if (*p == '"')
        in_string = true;
      else if (*p == '%') {
        *p = '\0';
        break;
      }
    }
  }
}

static bool has_complete_clause(const char *buf) {
  bool in_string = false;
  int depth = 0;
  for (const char *p = buf; *p; p++) {
    if (in_string) {
      if (*p == '\\' && *(p + 1))
        p++;
      else if (*p == '"')
        in_string = false;
    } else {
      if (*p == '"') {
        in_string = true;
      } else if (*p == '(' || *p == '[') {
        depth++;
      } else if (*p == ')' || *p == ']') {
        depth--;
      } else if (*p == '.' && depth == 0) {
        char next = *(p + 1);
        if (next == '\0' || isspace((unsigned char)next))
          return true;
      }
    }
  }
  return false;
}

bool prolog_exec_query(prolog_ctx_t *ctx, char *query) {
  parse_error_clear(ctx);
  ctx->input_ptr = query;
  ctx->input_start = query;

  int term_mark = ctx->term_count;
  int string_mark = ctx->string_pool_offset;
  int db_mark = ctx->db_count;

  goal_stmt_t goals = {0};
  do {
    skip_ws(ctx);
    term_t *g = parse_term(ctx);
    if (!g) {
      if (parse_has_error(ctx)) {
        parse_error_print(ctx);
        return false;
      }
      break;
    }
    if (goals.count < MAX_GOALS)
      goals.goals[goals.count++] = g;
    skip_ws(ctx);
  } while (*ctx->input_ptr == ',' && ctx->input_ptr++);

  if (goals.count == 0) {
    fprintf(stderr, "Error: empty query\n");
    return false;
  }

  env_t env = {0};
  bool ok = solve(ctx, &goals, &env);
  if (ok) {
    bool printed = false;
    for (int i = 0; i < env.count; i++) {
      char *name = env.bindings[i].name;
      if (strchr(name, '#'))
        continue;
      if (name[0] == '_')
        continue;
      if (printed)
        io_write_str(ctx, ", ");
      io_writef(ctx, "%s = ", name);
      io_write_term(ctx, env.bindings[i].value, &env);
      printed = true;
    }
    if (!printed)
      io_write_str(ctx, "true");
    io_write_str(ctx, "\n");
  } else {
    io_write_str(ctx, "false\n");
  }

  // restore pools if no new clauses were added
  // (clauses added via include must keep their terms)
  if (ctx->db_count == db_mark) {
    ctx->term_count = term_mark;
    ctx->string_pool_offset = string_mark;
  }

  return ok;
}

static void exec_directive(prolog_ctx_t *ctx, char *buf) {
  prolog_exec_query(ctx, buf + 2); // skip "?-"
}

bool prolog_load_file(prolog_ctx_t *ctx, const char *filename) {
  FILE *f = fopen(filename, "r");
  if (!f) {
    fprintf(stderr, "Error: cannot open file '%s'\n", filename);
    return false;
  }

  // set load_dir to this file's directory so nested includes resolve correctly
  char old_load_dir[MAX_FILE_PATH];
  strncpy(old_load_dir, ctx->load_dir, sizeof(old_load_dir) - 1);
  old_load_dir[sizeof(old_load_dir) - 1] = '\0';
  const char *last_slash = strrchr(filename, '/');
  if (last_slash) {
    size_t len = (size_t)(last_slash - filename);
    if (len >= sizeof(ctx->load_dir))
      len = sizeof(ctx->load_dir) - 1;
    strncpy(ctx->load_dir, filename, len);
    ctx->load_dir[len] = '\0';
  }

  char line[1024];
  char clause[16384] = {0};

  while (fgets(line, sizeof(line), f)) {
    line[strcspn(line, "\n")] = 0;
    strip_line_comment(line);

    char *trimmed = line;
    while (isspace((unsigned char)*trimmed))
      trimmed++;

    if (*trimmed == '\0' && clause[0] == '\0')
      continue;

    if (clause[0] != '\0' && *trimmed != '\0')
      strncat(clause, " ", sizeof(clause) - strlen(clause) - 1);
    strncat(clause, trimmed, sizeof(clause) - strlen(clause) - 1);

    if (has_complete_clause(clause)) {
      ctx->input_line++;
      if (strncmp(clause, "?-", 2) == 0)
        exec_directive(ctx, clause);
      else
        parse_clause(ctx, clause);
      clause[0] = '\0';
      if (parse_has_error(ctx)) {
        fclose(f);
        return false;
      }
    }
  }

  char *p = clause;
  while (isspace((unsigned char)*p))
    p++;
  if (*p != '\0')
    fprintf(stderr, "Warning: unterminated clause at end of '%s'\n", filename);

  strncpy(ctx->load_dir, old_load_dir, sizeof(ctx->load_dir) - 1);
  fclose(f);
  return true;
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

  // terminating dot
  skip_ws(ctx);
  if (*ctx->input_ptr != '.') {
    parse_error(ctx, "expected '.' at end of clause");
    parse_error_print(ctx);
    return;
  }
  ctx->input_ptr++;

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