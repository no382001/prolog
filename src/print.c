#include "platform_impl.h"

//****
//* operator classification
//****

// operators that print infix (arity 2) or prefix (arity 1)
static bool is_infix_op(const char *name) {
  static const char *ops[] = {
      "+", "-", "*",  "/",   "//",  "mod",  "is", "=",  "\\=", "==",  "\\==",
      "<", ">", "=<", ">=",  "=:=", "=\\=", "@<", "@>", "@=<", "@>=", "->",
      ";", ",", "^",  "=..", "\\/", "/\\",  ">>", "<<", "xor", NULL};
  for (const char **p = ops; *p; p++)
    if (strcmp(name, *p) == 0)
      return true;
  return false;
}

static bool is_prefix_op(const char *name) {
  return strcmp(name, "\\+") == 0 || strcmp(name, "not") == 0 ||
         strcmp(name, "\\") == 0;
}

// true if atom name needs single-quote wrapping
static bool needs_quoting(const char *name) {
  if (!name || name[0] == '\0')
    return true;
  // special atoms that never need quoting
  if (strcmp(name, "[]") == 0 || strcmp(name, "{}") == 0 ||
      strcmp(name, "!") == 0)
    return false;
  // atoms that look like integers need quoting (e.g. '1', '-42')
  {
    const char *p = name;
    if (*p == '-')
      p++;
    if (*p >= '0' && *p <= '9') {
      while (*p >= '0' && *p <= '9')
        p++;
      if (*p == '\0')
        return true;
    }
  }
  // starts with uppercase or underscore → variable-like, must quote
  if (isupper((unsigned char)name[0]) || name[0] == '_')
    return true;
  // contains whitespace or prolog comment/string delimiters → must quote
  for (const char *p = name; *p; p++) {
    if (isspace((unsigned char)*p) || *p == '%' || *p == '\'' || *p == '"')
      return true;
  }
  return false;
}

static void print_atom(trilog_ctx_t *ctx, const char *name, bool quoted) {
  if (quoted && needs_quoting(name)) {
    io_write_str(ctx, "'");
    for (const char *p = name; *p; p++) {
      if (*p == '\'')
        io_write_str(ctx, "\\'");
      else if (*p == '\\')
        io_write_str(ctx, "\\\\");
      else if (*p == '\n')
        io_write_str(ctx, "\\n");
      else if (*p == '\t')
        io_write_str(ctx, "\\t");
      else if (*p == '\r')
        io_write_str(ctx, "\\r");
      else {
        char buf[2] = {*p, '\0'};
        io_write_str(ctx, buf);
      }
    }
    io_write_str(ctx, "'");
  } else {
    io_write_str(ctx, name);
  }
}

//****
//* term printing
//****

// stable per-render name for an unnamed var (see anon_rename_active):
// first-seen var_id -> "_A", second -> "_B", ... "_Z", "_AA", "_AB", ...
static const char *anon_var_name(trilog_ctx_t *ctx, int var_id) {
  static char buf[16]; // reused per call; caller writes it out immediately
  int idx = -1;
  for (int i = 0; i < ctx->anon_rename_count; i++) {
    if (ctx->anon_rename_ids[i] == var_id) {
      idx = i;
      break;
    }
  }
  if (idx < 0 && ctx->anon_rename_count < MAX_VARS_ANON_RENAME) {
    idx = ctx->anon_rename_count;
    ctx->anon_rename_ids[idx] = var_id;
    ctx->anon_rename_count++;
  }
  if (idx < 0) {
    // table full: fall back to the raw internal name rather than crash
    snprintf(buf, sizeof(buf), "_G%d", var_id);
    return buf;
  }
  char letters[8];
  int len = 0;
  int n = idx + 1; // 1-based, spreadsheet-column-style base-26
  while (n > 0) {
    int rem = (n - 1) % 26;
    letters[len++] = (char)('A' + rem);
    n = (n - 1) / 26;
  }
  buf[0] = '_';
  int j = 1;
  for (int i = len - 1; i >= 0; i--)
    buf[j++] = letters[i];
  buf[j] = '\0';
  return buf;
}

void print_term(trilog_ctx_t *ctx, term_t *t, env_t *env, bool quoted) {
  assert(env != ((void *)0) && "Environment is NULL");

  if (!t) {
    io_write_str(ctx, "NULL");
    return;
  }

  t = deref(env, t);

  // packed string: print directly as double-quoted string
  if (t->type == STR) {
    if (t->arity == 0) {
      io_write_str(ctx, "[]");
      return;
    }
    io_write_str(ctx, "\"");
    for (int i = 0; i < t->arity; i++) {
      char c = t->name[i];
      if (c == '"' || c == '\\') {
        char esc[3] = {'\\', c, '\0'};
        io_write_str(ctx, esc);
      } else {
        bool found = false;
        for (const str_escape_t *e = STR_ESCAPES; e->raw; e++) {
          if (c == e->raw) {
            char esc[3] = {'\\', e->seq, '\0'};
            io_write_str(ctx, esc);
            found = true;
            break;
          }
        }
        if (!found) {
          char ch[2] = {c, '\0'};
          io_write_str(ctx, ch);
        }
      }
    }
    io_write_str(ctx, "\"");
    return;
  }

  if (is_cons(t)) {
    // print char lists as double-quoted strings (double_quotes = chars)
    term_t *scan = t;
    bool is_chars = true;
    while (is_cons(scan)) {
      term_t *h = deref(env, list_head(ctx, scan));
      if (h->type != CONST || !h->name || strlen(h->name) != 1) {
        is_chars = false;
        break;
      }
      scan = deref(env, list_tail(ctx, scan));
    }
    if (is_chars && is_nil(scan)) {
      io_write_str(ctx, "\"");
      while (is_cons(t)) {
        term_t *h = deref(env, list_head(ctx, t));
        char c = h->name[0];
        if (c == '"' || c == '\\') {
          char esc[3] = {'\\', c, '\0'};
          io_write_str(ctx, esc);
        } else if (c == '\n')
          io_write_str(ctx, "\\n");
        else if (c == '\t')
          io_write_str(ctx, "\\t");
        else if (c == '\r')
          io_write_str(ctx, "\\r");
        else {
          char ch[2] = {c, '\0'};
          io_write_str(ctx, ch);
        }
        t = deref(env, list_tail(ctx, t));
      }
      io_write_str(ctx, "\"");
      return;
    }

    io_write_str(ctx, "[");
    while (is_cons(t)) {
      print_term(ctx, list_head(ctx, t), env, quoted);
      t = deref(env, list_tail(ctx, t));
      if (is_cons(t))
        io_write_str(ctx, ", ");
    }
    if (!is_nil(t)) {
      io_write_str(ctx, "|");
      print_term(ctx, t, env, quoted);
    }
    io_write_str(ctx, "]");
    return;
  }

  if (is_nil(t)) {
    io_write_str(ctx, "[]");
    return;
  }

  if (t->type == VAR) {
    if (t->name)
      io_write_str(ctx, t->name);
    else if (ctx->anon_rename_active) {
      io_write_str(ctx, anon_var_name(ctx, t->arity));
    } else {
      char buf[32];
      snprintf(buf, sizeof(buf), "_G%d", t->arity);
      io_write_str(ctx, buf);
    }
    return;
  }

  // infix binary operator: x op y
  if (t->type == FUNC && t->arity == 2 && is_infix_op(t->name)) {
    print_term(ctx, t->args[0], env, quoted);
    io_write_str(ctx, t->name);
    print_term(ctx, t->args[1], env, quoted);
    return;
  }

  // prefix unary operator: op x
  if (t->type == FUNC && t->arity == 1 && is_prefix_op(t->name)) {
    io_write_str(ctx, t->name);
    io_write_str(ctx, "(");
    print_term(ctx, t->args[0], env, quoted);
    io_write_str(ctx, ")");
    return;
  }

  // INT/FLOAT terms always print unquoted (they are number literals, not atoms)
  print_atom(ctx, t->name, quoted && t->type != INT && t->type != FLOAT);
  if (t->type == FUNC && t->arity > 0) {
    io_write_str(ctx, "(");
    for (int i = 0; i < t->arity; i++) {
      if (i > 0)
        io_write_str(ctx, ", ");
      print_term(ctx, t->args[i], env, quoted);
    }
    io_write_str(ctx, ")");
  }
}

//****
//* binding output
//****

void print_bindings(trilog_ctx_t *ctx, env_t *env) {
  bool printed = false;
  ctx->anon_rename_active = true;
  ctx->anon_rename_count = 0;
  for (int i = 0; i < env->count; i++) {
    const char *name = env->bindings[i].name;
    if (!name || name[0] == '_')
      continue;
    if (printed)
      io_write_str(ctx, ", ");
    io_writef(ctx, "%s = ", name);
    io_write_term_quoted(ctx, env->bindings[i].value, env);
    printed = true;
  }
  ctx->anon_rename_active = false;
  if (!printed)
    io_write_str(ctx, "true");
}
