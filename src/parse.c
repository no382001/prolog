#include "platform_impl.h"

//****
//* error reporting
//****

void parse_error(trilog_ctx_t *ctx, const char *fmt, ...) {
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

void parse_error_clear(trilog_ctx_t *ctx) {
  ctx->error.has_error = false;
  ctx->error.error_is_eof = false;
  ctx->error.message[0] = '\0';
  ctx->error.line = 0;
  ctx->error.column = 0;
}

static void parse_error_eof(trilog_ctx_t *ctx) {
  if (ctx->error.has_error)
    return;
  ctx->error.has_error = true;
  ctx->error.error_is_eof = true;
  strncpy(ctx->error.message, "end_of_file", MAX_ERROR_MSG - 1);
  ctx->error.message[MAX_ERROR_MSG - 1] = '\0';
  ctx->error.line = ctx->input_line;
  ctx->error.column = (int)(ctx->input_ptr - ctx->input_start) + 1;
}

bool parse_has_error(trilog_ctx_t *ctx) { return ctx->error.has_error; }

void parse_error_print(trilog_ctx_t *ctx) {
  if (!ctx->error.has_error)
    return;

  if (ctx->input_start && ctx->error.column > 0) {
    // show the offending line and point to the error
    io_writef_err(ctx, "  %s\n", ctx->input_start);
    io_writef_err(ctx, "  %*s^\n", ctx->error.column - 1, "");
    io_writef_err(ctx, "error: %s\n", ctx->error.message);
  } else {
    // fallback for non-interactive / no context
    io_writef_err(ctx, "error: line %d, column %d: %s\n", ctx->error.line,
                  ctx->error.column, ctx->error.message);
  }
}

//****
//* whitespace and comment skipping
//****

void skip_ws(trilog_ctx_t *ctx) {
  assert(ctx != NULL && "Context is NULL");
  assert(ctx->input_ptr != NULL && "Input pointer is NULL");
  while (*ctx->input_ptr) {
    if (isspace((unsigned char)*ctx->input_ptr)) {
      ctx->input_ptr++;
    } else if (ctx->input_ptr[0] == '/' && ctx->input_ptr[1] == '*') {
      ctx->input_ptr += 2;
      while (*ctx->input_ptr &&
             !(ctx->input_ptr[0] == '*' && ctx->input_ptr[1] == '/'))
        ctx->input_ptr++;
      if (*ctx->input_ptr)
        ctx->input_ptr += 2;
    } else {
      break;
    }
  }
}

//****
//* operator table management
//****

// default operator table loaded into ctx at first use
typedef struct {
  const char *name;
  int priority;
  op_assoc_t assoc;
} op_default_t;

static const op_default_t default_ops[] = {
    {":-", 1200, OP_XFX},  {"-->", 1200, OP_XFX}, {":-", 1200, OP_FX},
    {"?-", 1200, OP_FX},   {";", 1100, OP_XFY},   {"->", 1050, OP_XFY},
    {",", 1000, OP_XFY},   {"\\+", 900, OP_FY},   {"not", 900, OP_FY},
    {"=", 700, OP_XFX},    {"\\=", 700, OP_XFX},  {"==", 700, OP_XFX},
    {"\\==", 700, OP_XFX}, {"@<", 700, OP_XFX},   {"@>", 700, OP_XFX},
    {"@=<", 700, OP_XFX},  {"@>=", 700, OP_XFX},  {"is", 700, OP_XFX},
    {"=..", 700, OP_XFX},  {"=:=", 700, OP_XFX},  {"=\\=", 700, OP_XFX},
    {"<", 700, OP_XFX},    {">", 700, OP_XFX},    {"=<", 700, OP_XFX},
    {">=", 700, OP_XFX},   {":", 600, OP_XFY},    {"+", 500, OP_YFX},
    {"-", 500, OP_YFX},    {"\\/", 500, OP_YFX},  {"xor", 400, OP_YFX},
    {"*", 400, OP_YFX},    {"/", 400, OP_YFX},    {"//", 400, OP_YFX},
    {"rem", 400, OP_YFX},  {"mod", 400, OP_YFX},  {"div", 400, OP_YFX},
    {"<<", 400, OP_YFX},   {">>", 400, OP_YFX},   {"/\\", 400, OP_YFX},
    {"**", 200, OP_XFX},   {"^", 200, OP_XFY},    {"-", 200, OP_FY},
    {"+", 200, OP_FY},     {"\\", 200, OP_FY},    {NULL, 0, OP_NONE}};

// initialise ctx op table from defaults (called on first parse if empty)
void ops_init_defaults(trilog_ctx_t *ctx) {
  if (ctx->op_count > 0)
    return;
  for (const op_default_t *d = default_ops; d->name; d++) {
    if (ctx->op_count >= MAX_OPS)
      break;
    op_entry_t *e = &ctx->op_table[ctx->op_count++];
    e->name = intern_name(ctx, d->name);
    e->priority = d->priority;
    e->assoc = d->assoc;
  }
}

// look up an infix/postfix operator; returns priority or 0
static int op_infix_priority(trilog_ctx_t *ctx, const char *name,
                             op_assoc_t *assoc_out) {
  for (int i = 0; i < ctx->op_count; i++) {
    op_entry_t *e = &ctx->op_table[i];
    if (e->priority == 0 || strcmp(e->name, name) != 0)
      continue;
    if (e->assoc == OP_XFX || e->assoc == OP_XFY || e->assoc == OP_YFX) {
      if (assoc_out)
        *assoc_out = e->assoc;
      return e->priority;
    }
  }
  return 0;
}

// look up a prefix operator; returns priority or 0
static int op_prefix_priority(trilog_ctx_t *ctx, const char *name,
                              op_assoc_t *assoc_out) {
  for (int i = 0; i < ctx->op_count; i++) {
    op_entry_t *e = &ctx->op_table[i];
    if (e->priority == 0 || strcmp(e->name, name) != 0)
      continue;
    if (e->assoc == OP_FX || e->assoc == OP_FY) {
      if (assoc_out)
        *assoc_out = e->assoc;
      return e->priority;
    }
  }
  return 0;
}

// attempt to scan an operator token at current input position.
// returns length consumed (0 = no operator found).
// writes the operator name into op_out (up to max_len-1 chars).
static int try_parse_op(trilog_ctx_t *ctx, char *op_out, int max_len) {
  char *p = ctx->input_ptr;
  int best_len = 0;
  const char *best = NULL;

  for (int i = 0; i < ctx->op_count; i++) {
    const char *n = ctx->op_table[i].name;
    if (!n || ctx->op_table[i].priority == 0)
      continue;
    int len = (int)strlen(n);
    if (len <= best_len)
      continue;
    if (strncmp(p, n, len) != 0)
      continue;
    // keyword operators need non-alnum after
    bool is_kw = isalpha((unsigned char)n[0]) || n[0] == '_';
    if (is_kw) {
      char next = p[len];
      if (isalnum((unsigned char)next) || next == '_')
        continue;
    }
    best_len = len;
    best = n;
  }

  if (!best)
    return 0;
  strncpy(op_out, best, max_len - 1);
  op_out[max_len - 1] = '\0';
  return best_len;
}

static term_t *parse_primary(trilog_ctx_t *ctx);
static term_t *parse_infix(trilog_ctx_t *ctx, term_t *left, int min_prec);
static term_t *parse_arg(trilog_ctx_t *ctx);

//****
//* list parsing
//****

term_t *parse_list(trilog_ctx_t *ctx) {
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

  term_t *elements[MAX_LIST_LIT]; // scratch buffer
  int count = 0;
  term_t *tail = NULL;

  elements[count] = parse_arg(ctx);
  if (!elements[count]) {
    if (*ctx->input_ptr == '\0' && !parse_has_error(ctx))
      parse_error_eof(ctx);
    else if (!parse_has_error(ctx))
      parse_error(ctx, "failed to parse list element");
    return NULL;
  }
  count++;
  skip_ws(ctx);

  while (*ctx->input_ptr == ',' || *ctx->input_ptr == '|') {
    if (*ctx->input_ptr == '|') {
      ctx->input_ptr++;
      skip_ws(ctx);
      tail = parse_arg(ctx);
      if (!tail) {
        if (*ctx->input_ptr == '\0' && !parse_has_error(ctx))
          parse_error_eof(ctx);
        else if (!parse_has_error(ctx))
          parse_error(ctx, "failed to parse list tail after '|'");
        return NULL;
      }
      skip_ws(ctx);
      break;
    }
    ctx->input_ptr++;
    skip_ws(ctx);

    if (count >= MAX_LIST_LIT) {
      parse_error(ctx, "too many list elements (max %d)", MAX_LIST_LIT);
      return NULL;
    }

    elements[count] = parse_arg(ctx);
    if (!elements[count]) {
      if (*ctx->input_ptr == '\0' && !parse_has_error(ctx))
        parse_error_eof(ctx);
      else if (!parse_has_error(ctx))
        parse_error(ctx, "failed to parse list element");
      return NULL;
    }
    count++;
    skip_ws(ctx);
  }

  if (*ctx->input_ptr != ']') {
    if (*ctx->input_ptr == '\0')
      parse_error_eof(ctx);
    else
      parse_error(ctx, "expected ']' to close list, got '%c'", *ctx->input_ptr);
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

//****
//* primary term parsing
//****

static term_t *parse_primary(trilog_ctx_t *ctx) {
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

  // parenthesized expression: (a) or (a, b, ...) conjunction
  if (*ctx->input_ptr == '(') {
    ctx->input_ptr++;
    skip_ws(ctx);
    term_t *inner = parse_term(ctx);
    if (!inner) {
      if (*ctx->input_ptr == '\0' && !parse_has_error(ctx))
        parse_error_eof(ctx);
      else if (!parse_has_error(ctx))
        parse_error(ctx, "expected expression inside parentheses");
      return NULL;
    }
    skip_ws(ctx);
    // ',' is now an infix operator, so parse_term above already consumed
    // any conjunction chain inside the parens.
    if (*ctx->input_ptr != ')') {
      if (*ctx->input_ptr == '\0')
        parse_error_eof(ctx);
      else
        parse_error(ctx, "expected ')' after expression, got '%c'",
                    *ctx->input_ptr);
      return NULL;
    }
    ctx->input_ptr++;
    return inner;
  }

  if (*ctx->input_ptr == '[')
    return parse_list(ctx);

  // '{}' is a valid atom (ISO solo character)
  if (ctx->input_ptr[0] == '{' && ctx->input_ptr[1] == '}') {
    ctx->input_ptr += 2;
    return make_const(ctx, "{}");
  }

  // '{Term}' parses as the compound term '{}'(Term) (ISO 6.3.3), used by
  // DCGs to embed a plain goal in a grammar rule body.
  if (*ctx->input_ptr == '{') {
    ctx->input_ptr++;
    skip_ws(ctx);
    term_t *inner = parse_term(ctx);
    if (!inner) {
      if (*ctx->input_ptr == '\0' && !parse_has_error(ctx))
        parse_error_eof(ctx);
      else if (!parse_has_error(ctx))
        parse_error(ctx, "expected expression inside '{...}'");
      return NULL;
    }
    skip_ws(ctx);
    if (*ctx->input_ptr != '}') {
      if (*ctx->input_ptr == '\0')
        parse_error_eof(ctx);
      else
        parse_error(ctx, "expected '}' after expression, got '%c'",
                    *ctx->input_ptr);
      return NULL;
    }
    ctx->input_ptr++;
    term_t *args[1] = {inner};
    return make_func(ctx, "{}", args, 1);
  }

  if (*ctx->input_ptr == '\'') {
    ctx->input_ptr++; // skip opening quote
    int avail = MAX_STRING_POOL - ctx->string_pool_offset - 1;
    char *name = ctx->string_pool + ctx->string_pool_offset;
    int i = 0;
    bool closed = false;
    while (*ctx->input_ptr) {
      if (*ctx->input_ptr == '\'') {
        if (ctx->input_ptr[1] == '\'') { // '' is escaped single-quote
          ctx->input_ptr += 2;
          if (i < avail)
            name[i++] = '\'';
        } else {
          ctx->input_ptr++; // closing quote
          closed = true;
          break;
        }
      } else if (*ctx->input_ptr == '\\' && ctx->input_ptr[1]) {
        // iso 6.4.2.1 escape sequences
        ctx->input_ptr++;
        char esc = *ctx->input_ptr++;
        if (i < avail) {
          switch (esc) {
          case '\\':
            name[i++] = '\\';
            break;
          case 'n':
            name[i++] = '\n';
            break;
          case 't':
            name[i++] = '\t';
            break;
          case 'a':
            name[i++] = '\a';
            break;
          case 'b':
            name[i++] = '\b';
            break;
          case 'r':
            name[i++] = '\r';
            break;
          case 'f':
            name[i++] = '\f';
            break;
          default:
            name[i++] = esc;
            break;
          }
        }
      } else {
        if (i < avail)
          name[i++] = *ctx->input_ptr;
        ctx->input_ptr++;
      }
    }
    name[i] = '\0';

    if (!closed) {
      parse_error_eof(ctx);
      return NULL;
    }

    // commit to string pool before parsing args (which may overwrite scratch)
    name = (char *)intern_name(ctx, name);

    // quoted atom followed by '(' is a functor call (iso 6.3.3)
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
          args[arity] = parse_arg(ctx);
          if (!args[arity]) {
            if (!parse_has_error(ctx)) {
              if (*ctx->input_ptr == '\0')
                parse_error_eof(ctx);
              else
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
        if (*ctx->input_ptr == '\0')
          parse_error_eof(ctx);
        else
          parse_error(ctx, "expected ')' after arguments, got '%c'",
                      *ctx->input_ptr);
        return NULL;
      }
      ctx->input_ptr++;
      return make_func(ctx, name, args, arity);
    }

    return make_const(ctx, name);
  }

  if (*ctx->input_ptr == '\"') {
    ctx->input_ptr++;           // skip opening quote
    char str_buf[MAX_NAME * 4]; // allow longer strings
    size_t i = 0;

    while (*ctx->input_ptr && *ctx->input_ptr != '\"') {
      if (i >= sizeof(str_buf) - 1) {
        parse_error(ctx, "string too long (max %d chars)",
                    (int)sizeof(str_buf) - 1);
        return NULL;
      }

      // escape sequences
      if (*ctx->input_ptr == '\\' && ctx->input_ptr[1]) {
        ctx->input_ptr++;
        char decoded = *ctx->input_ptr;
        for (const str_escape_t *e = STR_ESCAPES; e->raw; e++) {
          if (e->seq == *ctx->input_ptr) {
            decoded = e->raw;
            break;
          }
        }
        str_buf[i++] = decoded;
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

    // build packed string term
    const char *data = intern_name(ctx, str_buf);
    return make_str(ctx, data, (int)i);
  }

  int avail = MAX_STRING_POOL - ctx->string_pool_offset - 1;
  char *name = ctx->string_pool + ctx->string_pool_offset;
  int i = 0;
  name[0] = '\0';

  if (*ctx->input_ptr == '!' || *ctx->input_ptr == ';') {
    name[i++] = *ctx->input_ptr++;
  } else if (isdigit(*ctx->input_ptr) ||
             (*ctx->input_ptr == '-' && isdigit(ctx->input_ptr[1]))) {
    if (*ctx->input_ptr == '-')
      name[i++] = *ctx->input_ptr++;
    // 0'c character code notation
    if (ctx->input_ptr[0] == '0' && ctx->input_ptr[1] == '\'') {
      ctx->input_ptr += 2;
      unsigned char ch = (unsigned char)*ctx->input_ptr;
      if (ch == '\\' && ctx->input_ptr[1]) {
        ctx->input_ptr++;
        switch (*ctx->input_ptr) {
        case 'n':
          ch = '\n';
          break;
        case 't':
          ch = '\t';
          break;
        case 'r':
          ch = '\r';
          break;
        case '\\':
          ch = '\\';
          break;
        case '\'':
          ch = '\'';
          break;
        default:
          ch = (unsigned char)*ctx->input_ptr;
          break;
        }
      }
      ctx->input_ptr++;
      snprintf(name + i, avail - i + 1, "%d", (int)ch);
      i = strlen(name);
    } else {
      while (isdigit(*ctx->input_ptr)) {
        if (i >= avail) {
          parse_error(ctx, "number too long");
          return NULL;
        }
        name[i++] = *ctx->input_ptr++;
      }
    }
  } else if (isalpha(*ctx->input_ptr) || *ctx->input_ptr == '_') {
    while (isalnum(*ctx->input_ptr) || *ctx->input_ptr == '_') {
      if (i >= avail) {
        parse_error(ctx, "name too long");
        return NULL;
      }
      name[i++] = *ctx->input_ptr++;
    }
  } else if (strchr("#$&*+-./:<=>?@^~|\\", *ctx->input_ptr)) {
    // graphic/symbol atom: scan a run of graphic characters.
    // Stop before '.' followed by whitespace/eof (end-of-clause).
    while (strchr("#$&*+-./:<=>?@^~|\\", *ctx->input_ptr)) {
      char c = *ctx->input_ptr;
      char next = ctx->input_ptr[1];
      // '.' at end-of-clause terminates the atom
      if (c == '.' && (next == '\0' || isspace((unsigned char)next)))
        break;
      if (i >= avail) {
        parse_error(ctx, "name too long");
        return NULL;
      }
      name[i++] = *ctx->input_ptr++;
    }
    if (i == 0)
      return NULL;
  } else {
    // not a valid start of term
    return NULL;
  }

  name[i] = '\0';
  // commit to string pool before parsing args (which may overwrite scratch)
  name = (char *)intern_name(ctx, name);
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
        args[arity] = parse_arg(ctx);
        if (!args[arity]) {
          if (!parse_has_error(ctx)) {
            if (*ctx->input_ptr == '\0')
              parse_error_eof(ctx);
            else
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
      if (*ctx->input_ptr == '\0')
        parse_error_eof(ctx);
      else
        parse_error(ctx, "expected ')' after arguments, got '%c'",
                    *ctx->input_ptr);
      return NULL;
    }
    ctx->input_ptr++;

    debug(ctx, "DEBUG parse_primary: functor %s/%d\n", name, arity);
    return make_func(ctx, name, args, arity);
  }

  if (isupper(name[0]) || name[0] == '_') {
    debug(ctx, "DEBUG parse_primary: variable %s\n", name);
    // anonymous variable: each _ is distinct; never shared.
    if (name[0] == '_' && name[1] == '\0') {
      return make_var(ctx, "_", ctx->var_counter++);
    }
    // named variable: share var_id within the clause/query.
    const char *iname = intern_name(ctx, name);
    for (int i = 0; i < ctx->clause_var_count; i++) {
      if (ctx->clause_vars[i].name == iname)
        return make_var(ctx, iname, ctx->clause_vars[i].var_id);
    }
    assert(ctx->clause_var_count < MAX_CLAUSE_VARS &&
           "Too many variables in clause");
    int vid = ctx->var_counter++;
    ctx->clause_vars[ctx->clause_var_count].name = iname;
    ctx->clause_vars[ctx->clause_var_count].var_id = vid;
    ctx->clause_var_count++;
    return make_var(ctx, iname, vid);
  }
  debug(ctx, "DEBUG parse_primary: constant %s\n", name);
  // integer literal (digits only, or minus + digits)
  {
    const char *p = name;
    if (*p == '-')
      p++;
    if (*p >= '0' && *p <= '9') {
      const char *q = p;
      while (*q >= '0' && *q <= '9')
        q++;
      if (*q == '\0') {
        int v = 0;
        const char *pp = name;
        int sign = 1;
        if (*pp == '-') {
          sign = -1;
          pp++;
        }
        while (*pp)
          v = v * 10 + (*pp++ - '0');
        return make_int(ctx, sign * v);
      }
    }
  }
  // prefix operator: if this atom is a prefix op and is not followed by '('
  // (which would make it a functor call already handled above), apply it.
  // Skip if next char is end-of-clause '.', ')', ']', ',', or EOF.
  {
    char next = *ctx->input_ptr;
    bool at_term_boundary =
        (next == '\0' || next == ')' || next == ']' || next == ',' ||
         next == '|' ||
         (next == '.' && (ctx->input_ptr[1] == '\0' ||
                          isspace((unsigned char)ctx->input_ptr[1]))));
    if (!at_term_boundary) {
      op_assoc_t passoc = OP_NONE;
      int pprec = op_prefix_priority(ctx, name, &passoc);
      if (pprec > 0) {
        // right-operand max: fy → same prec, fx → prec-1
        int arg_max = (passoc == OP_FY) ? pprec : pprec - 1;
        term_t *inner = parse_primary(ctx);
        if (!inner) {
          if (!parse_has_error(ctx)) {
            if (*ctx->input_ptr == '\0')
              parse_error_eof(ctx);
            else
              parse_error(ctx, "expected term after prefix '%s'", name);
          }
          return NULL;
        }
        inner = parse_infix(ctx, inner, arg_max);
        if (!inner)
          return NULL;
        term_t *args[1] = {inner};
        return make_func(ctx, name, args, 1);
      }
    }
  }
  return make_const(ctx, name);
}

//****
//* infix operator parsing
//****

// parse_infix: Prolog Pratt parser using MAX_PREC semantics.
// In Prolog, higher priority number = looser binding (wider scope).
// max_prec: only consume operators with priority <= max_prec.
//   parse_term: max_prec = 1200 (accept all)
//   parse_arg:  max_prec = 999  (stop before ',' at 1000)
static term_t *parse_infix(trilog_ctx_t *ctx, term_t *left, int max_prec) {
  while (1) {
    skip_ws(ctx);

    char op[MAX_NAME] = {0};
    int op_len = try_parse_op(ctx, op, sizeof(op));

    if (op_len == 0)
      return left;

    op_assoc_t assoc = OP_NONE;
    int prec = op_infix_priority(ctx, op, &assoc);
    // Skip if not an infix op or priority is too high (looser than allowed)
    if (prec == 0 || prec > max_prec)
      return left;

    ctx->input_ptr += op_len;
    skip_ws(ctx);

    // right-operand max precedence (lower = tighter in Prolog):
    //   yfx (left-assoc):  right < prec  → prec-1
    //   xfy (right-assoc): right <= prec → prec
    //   xfx (non-assoc):   right < prec  → prec-1
    int right_max = (assoc == OP_XFY) ? prec : prec - 1;

    term_t *right = parse_primary(ctx);
    if (!right) {
      if (*ctx->input_ptr == '\0' && !parse_has_error(ctx))
        parse_error_eof(ctx);
      else if (!parse_has_error(ctx))
        parse_error(ctx, "expected term after '%s'", op);
      return NULL;
    }

    // Look ahead: recursively consume any right-side operators that are
    // allowed as the right operand (priority <= right_max).
    skip_ws(ctx);
    char next_op[MAX_NAME] = {0};
    int next_len = try_parse_op(ctx, next_op, sizeof(next_op));

    while (next_len > 0) {
      op_assoc_t next_assoc = OP_NONE;
      int next_prec = op_infix_priority(ctx, next_op, &next_assoc);
      if (next_prec == 0 || next_prec > right_max)
        break;
      right = parse_infix(ctx, right, right_max);
      if (!right)
        return NULL;
      skip_ws(ctx);
      next_len = try_parse_op(ctx, next_op, sizeof(next_op));
    }

    term_t *args[2] = {left, right};
    left = make_func(ctx, op, args, 2);
  }
}

term_t *parse_term(trilog_ctx_t *ctx) {
  assert(ctx != NULL && "Context is NULL");
  assert(ctx->input_ptr != NULL && "Input pointer is NULL");

  ops_init_defaults(ctx);

  if (parse_has_error(ctx))
    return NULL;

  term_t *left = parse_primary(ctx);
  if (!left)
    return NULL;

  return parse_infix(ctx, left, 1200);
}

// parse_arg: parses a functor argument or list element.
// ISO 6.3.3.1: argument priority < 1000, so stop before ',' at 1000.
static term_t *parse_arg(trilog_ctx_t *ctx) {
  term_t *left = parse_primary(ctx);
  if (!left)
    return NULL;
  return parse_infix(ctx, left, 999);
}

//****
//* comment stripping and clause detection
//****

void strip_line_comment(char *line) {
  bool in_dq = false, in_sq = false;
  for (char *p = line; *p; p++) {
    if (in_dq) {
      if (*p == '\\' && *(p + 1))
        p++;
      else if (*p == '"')
        in_dq = false;
    } else if (in_sq) {
      if (*p == '\\' && *(p + 1))
        p++; // backslash escape (e.g. \')
      else if (*p == '\'' && *(p + 1) == '\'')
        p++; // escaped ''
      else if (*p == '\'')
        in_sq = false;
    } else {
      if (*p == '"')
        in_dq = true;
      else if (*p == '\'')
        in_sq = true;
      else if (*p == '%') {
        *p = '\0';
        break;
      }
    }
  }
}

bool has_complete_clause(const char *buf) {
  bool in_dq = false, in_sq = false;
  int depth = 0;
  for (const char *p = buf; *p; p++) {
    if (in_dq) {
      if (*p == '\\' && *(p + 1))
        p++;
      else if (*p == '"')
        in_dq = false;
    } else if (in_sq) {
      if (*p == '\\' && *(p + 1))
        p++; // backslash escape (e.g. \')
      else if (*p == '\'' && *(p + 1) == '\'')
        p++; // doubled-quote escape ''
      else if (*p == '\'')
        in_sq = false;
    } else {
      if (*p == '"') {
        in_dq = true;
      } else if (*p == '\'') {
        in_sq = true;
      } else if (*p == '(' || *p == '[') {
        depth++;
      } else if (*p == ')' || *p == ']') {
        depth--;
      } else if (*p == '.' && depth == 0) {
        // a '.' immediately preceded by another '.' is part of a multi-dot
        // operator token (e.g. "=.." for univ), not a clause terminator.
        char prev = (p == buf) ? '\0' : *(p - 1);
        char next = *(p + 1);
        if (prev != '.' && (next == '\0' || isspace((unsigned char)next)))
          return true;
      }
    }
  }
  return false;
}

//****
//* query execution
//****

static bool parse_goals(trilog_ctx_t *ctx, char *query, goal_stmt_t *goals) {
  parse_error_clear(ctx);
  ctx->has_runtime_error = false;
  ctx->input_ptr = query;
  ctx->input_start = query;
  ctx->clause_var_count = 0;

  // collect terms into a temporary stack buffer, then allocate from pool
  term_t *tmp[MAX_GOALS];
  int n = 0;
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
    if (n < MAX_GOALS)
      tmp[n++] = g;
    skip_ws(ctx);
  } while (*ctx->input_ptr == ',' && ctx->input_ptr++);

  if (n == 0) {
    io_writef_err(ctx, "Error: empty query\n");
    return false;
  }

  // check for trailing unparsed input (skip optional trailing '.')
  skip_ws(ctx);
  if (*ctx->input_ptr == '.')
    ctx->input_ptr++;
  skip_ws(ctx);
  if (*ctx->input_ptr != '\0') {
    parse_error(ctx, "unexpected token: '%c'", *ctx->input_ptr);
    parse_error_print(ctx);
    return false;
  }

  *goals = goals_alloc(ctx, n);
  for (int i = 0; i < n; i++)
    goals->goals[goals->count++] = tmp[i];
  return true;
}

bool trilog_exec_query(trilog_ctx_t *ctx, char *query) {
  int term_mark = ctx->term_pool_offset;
  int string_mark = ctx->string_pool_offset;
  int db_mark = ctx->db_count;
  ctx->db_dirty = false;

  goal_stmt_t goals = {0};
  if (!parse_goals(ctx, query, &goals))
    return false;

  ctx->bind_count = 0;
  env_t env = {.bindings = ctx->bindings, .count = 0};
  bool ok = solve(ctx, &goals, &env);

  if (ctx->has_runtime_error) {
    if (ctx->thrown_ball) {
      env_t err_env = {.bindings = ctx->bindings, .count = 0};
      term_t *ball = ctx->thrown_ball;
      if (ball->type == FUNC && ball->arity == 2 &&
          strcmp(ball->name, "error") == 0)
        ball = ball->args[0];
      io_write_str(ctx, "   ");
      io_write_term_quoted(ctx, ball, &err_env);
      io_write_str(ctx, ".\n");
    }
    ctx->has_runtime_error = false;
    ok = false;
  } else if (ok) {
    print_bindings(ctx, &env);
    io_write_str(ctx, "\n");
  } else {
    io_write_str(ctx, "false\n");
  }

  // restore pools if no database or op table modifications happened
  if (!ctx->db_dirty && !ctx->ops_dirty && ctx->db_count == db_mark) {
    ctx->term_pool_offset = term_mark;
    ctx->string_pool_offset = string_mark;
  }
  ctx->db_dirty = false;
  ctx->ops_dirty = false;
  return ok;
}

// parse and run a query, calling cb for each solution (no printing).
// returns true if at least one solution was found.
bool trilog_exec_query_multi(trilog_ctx_t *ctx, char *query,
                             solution_callback_t cb, void *ud) {
  int term_mark = ctx->term_pool_offset;
  int string_mark = ctx->string_pool_offset;
  int db_mark = ctx->db_count;
  ctx->db_dirty = false;

  goal_stmt_t goals = {0};
  if (!parse_goals(ctx, query, &goals))
    return false;

  ctx->bind_count = 0;
  env_t env = {.bindings = ctx->bindings, .count = 0};
  bool found = solve_all(ctx, &goals, &env, cb, ud);

  if (ctx->has_runtime_error) {
    if (ctx->thrown_ball) {
      env_t err_env = {.bindings = ctx->bindings, .count = 0};
      term_t *ball = ctx->thrown_ball;
      if (ball->type == FUNC && ball->arity == 2 &&
          strcmp(ball->name, "error") == 0)
        ball = ball->args[0];
      io_write_str(ctx, "   ");
      io_write_term_quoted(ctx, ball, &err_env);
      io_write_str(ctx, ".\n");
    }
    // leave has_runtime_error set so the caller can suppress "false"
    found = false;
  }

  if (!ctx->db_dirty && !ctx->ops_dirty && ctx->db_count == db_mark) {
    ctx->term_pool_offset = term_mark;
    ctx->string_pool_offset = string_mark;
  }
  ctx->db_dirty = false;
  ctx->ops_dirty = false;
  return found;
}

//****
//* file and string loading
//****

static void exec_directive(trilog_ctx_t *ctx, char *buf) {
  trilog_exec_query_multi(ctx, buf + 2, NULL, NULL); // silent
  ctx->has_runtime_error = false;
}

// accumulate one trimmed line into clause[]. if a complete clause is ready,
// dispatch it and reset. returns false on parse error.
static bool process_clause_line(trilog_ctx_t *ctx, char *clause, size_t sz,
                                const char *trimmed) {
  if (*trimmed == '\0' && clause[0] == '\0')
    return true;
  if (clause[0] != '\0' && *trimmed != '\0')
    strncat(clause, " ", sz - strlen(clause) - 1);
  strncat(clause, trimmed, sz - strlen(clause) - 1);

  if (!has_complete_clause(clause))
    return true;

  ctx->input_line++;
  if (strncmp(clause, "?-", 2) == 0 || strncmp(clause, ":-", 2) == 0)
    exec_directive(ctx, clause);
  else
    parse_clause(ctx, clause);
  clause[0] = '\0';
  return !parse_has_error(ctx);
}

static bool load_clauses_from_fp(trilog_ctx_t *ctx, void *f,
                                 const char *label) {
  char line[1024];
  char clause[16384] = {0};

  while (io_file_read_line(ctx, f, line, sizeof(line))) {
    line[strcspn(line, "\n")] = 0;
    strip_line_comment(line);
    char *trimmed = line;
    while (isspace((unsigned char)*trimmed))
      trimmed++;
    if (!process_clause_line(ctx, clause, sizeof(clause), trimmed))
      return false;
  }

  char *p = clause;
  while (isspace((unsigned char)*p))
    p++;
  if (*p != '\0')
    io_writef_err(ctx, "Warning: unterminated clause at end of '%s'\n", label);
  return true;
}

bool trilog_load_file(trilog_ctx_t *ctx, const char *filename) {
  void *f = io_file_open(ctx, filename, "r");
  if (!f) {
    io_writef_err(ctx, "Error: cannot open file '%s'\n", filename);
    return false;
  }

  // track top-level files for make/0 and handle re-consult
  // compare by basename so that "/path/to/core.pl" and "core.pl" match
  if (ctx->include_depth == 0) {
    const char *base = strrchr(filename, '/');
    base = base ? base + 1 : filename;
    int file_idx = -1;
    for (int i = 0; i < ctx->make_file_count; i++) {
      const char *tbase = strrchr(ctx->make_files[i].path, '/');
      tbase = tbase ? tbase + 1 : ctx->make_files[i].path;
      if (strcmp(tbase, base) == 0) {
        file_idx = i;
        break;
      }
    }
    if (file_idx >= 0) {
      // re-consult: skip if file hasn't changed (avoids leaking perm pool)
      long long cur_mtime = io_file_mtime(ctx, filename);
      if (cur_mtime != -1LL && cur_mtime == ctx->make_files[file_idx].mtime) {
        io_file_close(ctx, f);
        return true;
      }
      // file changed: remove old clauses and reload
      int dst = 0;
      for (int src = 0; src < ctx->db_count; src++) {
        if (ctx->database[src].source_file != file_idx)
          ctx->database[dst++] = ctx->database[src];
      }
      ctx->db_count = dst;
      ctx->make_files[file_idx].mtime = cur_mtime;
      ctx->current_source_file = file_idx;
    } else {
      if (ctx->make_file_count == 0) {
        // first file ever (or first after a make reset): snapshot state
        ctx->make_db_mark = ctx->db_count;
        ctx->make_term_mark = ctx->term_pool_offset;
        ctx->make_string_mark = ctx->string_pool_offset;
      }
      if (ctx->make_file_count < MAX_MAKE_FILES) {
        int idx = ctx->make_file_count++;
        strncpy(ctx->make_files[idx].path, filename, MAX_FILE_PATH - 1);
        ctx->make_files[idx].path[MAX_FILE_PATH - 1] = '\0';
        ctx->make_files[idx].mtime = io_file_mtime(ctx, filename);
        ctx->current_source_file = idx;
      }
    }
  }

  ctx->include_depth++;

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

  bool ok = load_clauses_from_fp(ctx, f, filename);

  strncpy(ctx->load_dir, old_load_dir, sizeof(ctx->load_dir) - 1);
  io_file_close(ctx, f);
  ctx->include_depth--;
  ctx->current_source_file = -1;
  return ok;
}

bool trilog_load_string(trilog_ctx_t *ctx, const char *src) {
  char line[1024];
  char clause[16384] = {0};
  ctx->include_depth++; // not tracked for make/0

  while (*src) {
    int i = 0;
    while (*src && *src != '\n' && i < (int)sizeof(line) - 1)
      line[i++] = *src++;
    if (*src == '\n')
      src++;
    line[i] = '\0';

    strip_line_comment(line);
    char *trimmed = line;
    while (isspace((unsigned char)*trimmed))
      trimmed++;
    if (!process_clause_line(ctx, clause, sizeof(clause), trimmed)) {
      ctx->include_depth--;
      return false;
    }
  }

  ctx->include_depth--;
  return true;
}

//****
//* clause parsing
//****

void parse_clause(trilog_ctx_t *ctx, char *line) {
  assert(ctx != NULL && "Context is NULL");
  assert(line != NULL && "Line cannot be NULL");

  parse_error_clear(ctx);
  ctx->input_ptr = line;
  ctx->input_start = line;
  ctx->clause_var_count = 0;

  if (ctx->db_count >= MAX_CLAUSES) {
    parse_error(ctx, "database full (max %d clauses)", MAX_CLAUSES);
    parse_error_print(ctx);
    return;
  }

  clause_t *c = &ctx->database[ctx->db_count];
  debug(ctx, "=== Parsing clause ===\n");

  ctx->alloc_permanent = true;
  term_t *whole = parse_term(ctx);
  if (!whole) {
    ctx->alloc_permanent = false;
    if (!parse_has_error(ctx)) {
      parse_error(ctx, "failed to parse clause head");
    }
    parse_error_print(ctx);
    return;
  }
  c->body_count = 0;
  c->body = NULL;

  // parse_term may have consumed 'Head :- Body' as a single :-/2 term
  // (since :- is a 1200 xfx infix operator).  Split it here.
  if (whole->type == FUNC && strcmp(whole->name, ":-") == 0 &&
      whole->arity == 2) {
    c->head = whole->args[0];
    term_t *body_term = whole->args[1];
    // Store body as a single goal; the solver flattens ','(A,B) via conjunction
    c->body = (term_t **)term_alloc(ctx, sizeof(term_t *));
    c->body[c->body_count++] = body_term;
  } else {
    c->head = whole;
  }

  // terminating dot
  skip_ws(ctx);
  if (*ctx->input_ptr != '.') {
    ctx->alloc_permanent = false;
    parse_error(ctx, "expected '.' at end of clause");
    parse_error_print(ctx);
    return;
  }
  ctx->input_ptr++;
  ctx->alloc_permanent = false;

  c->source_file = ctx->current_source_file;
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