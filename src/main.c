#define _POSIX_C_SOURCE 200809L
#include "platform_impl.h"
#include <getopt.h>
#include <libgen.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define CORE_PATH_MAX 8192

static builtin_result_t ffi_get_time_ms(trilog_ctx_t *ctx, term_t *goal,
                                        env_t *env) {
  if (goal->arity != 1)
    return BUILTIN_FAIL;

  static long long epoch_ms = -1;
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  long long now_ms = (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
  if (epoch_ms < 0)
    epoch_ms = now_ms;

  term_t *t = make_int(ctx, (int)(now_ms - epoch_ms));
  return unify(ctx, goal->args[0], t, env) ? BUILTIN_OK : BUILTIN_FAIL;
}

//****
//* core library loading
//****

static void consult_path(trilog_ctx_t *ctx, const char *path) {
  char query[CORE_PATH_MAX + 16];
  snprintf(query, sizeof(query), "consult('%s').", path);
  io_write_str(ctx, "?- ");
  io_write_str(ctx, query);
  io_write_str(ctx, "\n");
  toplevel_query(ctx, query);
}

static void try_load_core(trilog_ctx_t *ctx, const char *argv0) {
  char exe[CORE_PATH_MAX];
  char dir[CORE_PATH_MAX];

  ssize_t len = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
  if (len > 0) {
    exe[len] = '\0';
    strncpy(dir, exe, sizeof(dir) - 1);
    dir[sizeof(dir) - 1] = '\0';
  } else {
    strncpy(dir, argv0, sizeof(dir) - 1);
    dir[sizeof(dir) - 1] = '\0';
  }
  dirname(dir);

  char path[CORE_PATH_MAX];
  strncpy(path, dir, sizeof(path) - 13);
  path[sizeof(path) - 13] = '\0';
  strcat(path, "/lib/core.pl");

  consult_path(ctx, path);
}

static void try_load_init_file(trilog_ctx_t *ctx) {
  const char *home = getenv("HOME");
  if (!home)
    return;

  char path[CORE_PATH_MAX];
  snprintf(path, sizeof(path), "%s/.trilog", home);

  // if (io_file_exists(ctx, path))
  consult_path(ctx, path);
}

//****
//* terminal and usage
//****

// raw single-keypress for interactive solution prompting
static int read_key_hook(trilog_ctx_t *ctx, void *ud) {
  (void)ctx;
  (void)ud;
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

static void print_usage(trilog_ctx_t *ctx, const char *prog) {
  io_writef_err(ctx, "Usage: %s [-d] [-s] [-f] [-e <expression>] [file.pl]\n",
                prog);
  io_writef_err(ctx, "  file.pl       Load clauses from file\n");
  io_writef_err(ctx, "  -d            Enable debug mode\n");
  io_writef_err(ctx, "  -s            Print stats to stderr on exit\n");
  io_writef_err(ctx, "  -f            Fast startup: do not load ~/.trilog\n");
  io_writef_err(ctx, "  -e <expr>     Execute expression and exit\n");
  io_writef_err(ctx, "  -h            Show this help\n");
  io_writef_err(ctx, "\nInteractive commands:\n");
  io_writef_err(ctx, "  debug.        Toggle debug mode\n");
  io_writef_err(ctx, "  halt.         Exit the interpreter\n");
}

static void print_exit_stats(trilog_ctx_t *ctx) {
  int perm = ctx->term_pool_size - ctx->term_pool_perm;
  fprintf(stderr, "perm_pool=%d\n", perm);
  fprintf(stderr, "string_pool=%d\n", ctx->string_pool_offset);
  fprintf(stderr, "clauses=%d\n", ctx->db_count);
  fprintf(stderr, "term_pool_peak=%d\n", ctx->term_pool_peak);
  fprintf(stderr, "var_counter=%d\n", ctx->var_counter);
  fprintf(stderr, "bind_count=%d\n", ctx->bind_count);
}

static void process_line(trilog_ctx_t *ctx, char *line, bool *should_exit,
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
    io_writef(ctx, "Debug mode %s\n",
              ctx->debug_enabled ? "enabled" : "disabled");
    return;
  }

  parse_error_clear(ctx);
  ctx->input_line++;

  if (interactive)
    exec_query_interactive(ctx, line);
  else
    toplevel_query(ctx, (strncmp(line, "?-", 2) == 0) ? line + 2 : line);
}

static bool load_file(trilog_ctx_t *ctx, const char *filename) {
  return trilog_load_file(ctx, filename);
}

//****
//* entry point
//****

int main(int argc, char *argv[]) {
  trilog_ctx_t *ctx = malloc(TRILOG_CTX_SIZE(TERM_POOL_BYTES));
  if (!ctx) {
    fprintf(stderr, "Fatal: failed to allocate trilog context\n");
    return 1;
  }
  trilog_ctx_init(ctx, TERM_POOL_BYTES);
  ops_init_defaults(
      ctx); // pre-load op table so names are below any query's string_mark

  io_hooks_init_default(ctx);

  ffi_register_builtin(ctx, "get_time_ms", 1, ffi_get_time_ms, NULL);

  // raw single-keypress for interactive solution prompting
  io_hooks_t hooks = {0};
  hooks.read_char = read_key_hook;
  io_hooks_set(ctx, &hooks);

  const char *input_file = NULL;
  const char *expression = NULL;
  bool exit_stats = false;
  bool fast_startup = false;
  int opt;

  while ((opt = getopt(argc, argv, "dsfe:h")) != -1) {
    switch (opt) {
    case 'd':
      ctx->debug_enabled = true;
      io_writef_err(ctx, "Debug mode enabled\n");
      break;
    case 's':
      exit_stats = true;
      break;
    case 'f':
      fast_startup = true;
      break;
    case 'e':
      expression = optarg;
      break;
    case 'h':
      print_usage(ctx, argv[0]);
      free(ctx);
      return 0;
    default:
      print_usage(ctx, argv[0]);
      free(ctx);
      return 1;
    }
  }

  if (optind < argc)
    input_file = argv[optind];

  try_load_core(ctx, argv[0]);
  if (!fast_startup)
    try_load_init_file(ctx);

  if (input_file) {
    if (!load_file(ctx, input_file)) {
      free(ctx);
      return 1;
    }
  }

  if (expression) {
    char line[1024];
    strncpy(line, expression, sizeof(line) - 1);
    bool should_exit = false;
    process_line(ctx, line, &should_exit, false);
    int rc = parse_has_error(ctx) ? 1 : 0;
    if (exit_stats)
      print_exit_stats(ctx);
    free(ctx);
    return rc;
  }

  char line[1024];
  bool interactive = isatty(STDIN_FILENO);
  bool should_exit = false;

  while (!should_exit) {
    if (interactive) {
      io_write_str(ctx, "?- ");
      fflush(stdout);
    }

    if (!fgets(line, sizeof(line), stdin))
      break;
    process_line(ctx, line, &should_exit, interactive);
  }

  if (exit_stats)
    print_exit_stats(ctx);
  free(ctx);
  return 0;
}