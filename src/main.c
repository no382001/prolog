#include "platform_impl.h"
#include <getopt.h>
#include <termios.h>
#include <unistd.h>

static int read_key(void) {
  struct termios old, raw;
  tcgetattr(STDIN_FILENO, &old);
  raw = old;
  raw.c_lflag &= ~(ICANON | ECHO);
  raw.c_cc[VMIN] = 1;
  raw.c_cc[VTIME] = 0;
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
  int c = getchar();
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &old);
  return c;
}

static void print_usage(const char *prog) {
  fprintf(stderr, "Usage: %s [-d] [-f <file>] [-e <expression>]\n", prog);
  fprintf(stderr, "  -d            Enable debug mode\n");
  fprintf(stderr, "  -f <file>     Load clauses from file\n");
  fprintf(stderr, "  -e <expr>     Execute expression and exit\n");
  fprintf(stderr, "  -h            Show this help\n");
  fprintf(stderr, "\nInteractive commands:\n");
  fprintf(stderr, "  debug.        Toggle debug mode\n");
  fprintf(stderr, "  halt.         Exit the interpreter\n");
}

typedef struct {
  bool interactive;
  bool want_more; // true if user typed ; on the last solution
} toplevel_state_t;

static void print_bindings(prolog_ctx_t *ctx, env_t *env) {
  bool printed = false;
  for (int i = 0; i < env->count; i++) {
    char *name = env->bindings[i].name;
    if (strchr(name, '#'))
      continue;
    if (name[0] == '_')
      continue;
    if (printed)
      io_write_str(ctx, ", ");
    io_writef(ctx, "%s = ", name);
    io_write_term_quoted(ctx, env->bindings[i].value, env);
    printed = true;
  }
  if (!printed)
    io_write_str(ctx, "true");
}

static bool toplevel_cb(prolog_ctx_t *ctx, env_t *env, void *ud,
                        bool has_more) {
  toplevel_state_t *st = ud;
  print_bindings(ctx, env);
  st->want_more = false;

  if (!st->interactive || !has_more) {
    io_write_str(ctx, "\n");
    return false; // no choice points: stop here
  }

  io_write_str(ctx, " ;");
  int c = read_key();
  io_write_str(ctx, "\n");
  st->want_more = (c == ';');
  return st->want_more;
}

static void exec_query(prolog_ctx_t *ctx, char *query, bool interactive) {
  toplevel_state_t st = {.interactive = interactive, .want_more = false};
  bool found = prolog_exec_query_multi(ctx, query, toplevel_cb, &st);
  if (!found || st.want_more)
    io_write_str(ctx, "false\n");
}

static void process_line(prolog_ctx_t *ctx, char *line, bool *should_exit,
                         bool interactive) {
  line[strcspn(line, "\n")] = 0;
  if (strlen(line) == 0)
    return;
  if (strcmp(line, "halt.") == 0) {
    *should_exit = true;
    return;
  }

  if (strcmp(line, "debug.") == 0) {
    ctx->debug_enabled = !ctx->debug_enabled;
    printf("Debug mode %s\n", ctx->debug_enabled ? "enabled" : "disabled");
    return;
  }

  parse_error_clear(ctx);
  ctx->input_line++;

  char *query = (strncmp(line, "?-", 2) == 0) ? line + 2 : line;
  exec_query(ctx, query, interactive);
}

static bool load_file(prolog_ctx_t *ctx, const char *filename) {
  return prolog_load_file(ctx, filename);
}

int main(int argc, char *argv[]) {
  prolog_ctx_t context = {0};
  prolog_ctx_t *ctx = &context;

  io_hooks_init_default(ctx);

  const char *input_file = NULL;
  const char *expression = NULL;
  int opt;

  while ((opt = getopt(argc, argv, "df:e:h")) != -1) {
    switch (opt) {
    case 'd':
      ctx->debug_enabled = true;
      fprintf(stderr, "Debug mode enabled\n");
      break;
    case 'f':
      input_file = optarg;
      break;
    case 'e':
      expression = optarg;
      break;
    case 'h':
      print_usage(argv[0]);
      return 0;
    default:
      print_usage(argv[0]);
      return 1;
    }
  }

  if (input_file) {
    if (!load_file(ctx, input_file)) {
      return 1;
    }
  }

  if (expression) {
    char line[1024];
    strncpy(line, expression, sizeof(line) - 1);
    bool should_exit = false;
    process_line(ctx, line, &should_exit, false);
    return parse_has_error(ctx) ? 1 : 0;
  }

  char line[1024];
  bool interactive = isatty(STDIN_FILENO);
  bool should_exit = false;

  while (!should_exit) {
    if (interactive) {
      printf("?- ");
      fflush(stdout);
    }

    if (!fgets(line, sizeof(line), stdin))
      break;
    process_line(ctx, line, &should_exit, interactive);
  }

  return 0;
}