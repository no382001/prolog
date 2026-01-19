#include "platform_impl.h"
#include <getopt.h>
#include <unistd.h>

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

static void process_line(prolog_ctx_t *ctx, char *line, bool *should_exit) {
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

  if (strncmp(line, "?-", 2) == 0) {
    // mark pools
    int term_mark = ctx->term_count;
    int string_mark = ctx->string_pool_offset;

    ctx->input_ptr = line + 2;
    ctx->input_start = line;

    debug(ctx, "=== Parsing query ===\n");
    goal_stmt_t goals = {0};
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
      if (goals.count >= MAX_GOALS) {
        parse_error(ctx, "too many goals in query (max %d)", MAX_GOALS);
        parse_error_print(ctx);
        return;
      }
      goals.goals[goals.count++] = g;
      skip_ws(ctx);
    } while (*ctx->input_ptr == ',' && ctx->input_ptr++);

    if (goals.count == 0) {
      fprintf(stderr, "Error: empty query\n");
      return;
    }

    if (ctx->debug_enabled) {
      debug(ctx, "=== Query has %d goals ===\n", goals.count);
      for (int i = 0; i < goals.count; i++) {
        debug(ctx, "QUERY[%d]: ", i);
        debug_term_raw(ctx, goals.goals[i]);
        debug(ctx, "\n");
      }
    }

    env_t env = {0};

    debug(ctx, "\n========== SOLVING ==========\n");
    if (solve(ctx, &goals, &env)) {
      if (ctx->debug_enabled) {
        debug(ctx, "\n========== RESULT ==========\n");
      }
      bool printed = false;
      for (int i = 0; i < env.count; i++) {
        char *name = env.bindings[i].name;
        assert(name != NULL && "Binding name is NULL");

        if (strchr(name, '#'))
          continue; // for internal use
        if (name[0] == '_')
          continue;

        if (printed)
          printf(", ");
        printf("%s = ", name);
        print_term(ctx, env.bindings[i].value, &env);
        printed = true;
      }
      if (!printed)
        printf("true");
      printf("\n");
    } else {
      printf("false\n");
    }

    // restore pools
    ctx->term_count = term_mark;
    ctx->string_pool_offset = string_mark;
  } else {
    parse_clause(ctx, line);
  }
}

static bool load_file(prolog_ctx_t *ctx, const char *filename) {
  FILE *f = fopen(filename, "r");
  if (!f) {
    fprintf(stderr, "Error: cannot open file '%s'\n", filename);
    return false;
  }

  char line[1024];
  bool should_exit = false;
  while (fgets(line, sizeof(line), f) && !should_exit) {
    process_line(ctx, line, &should_exit);
    if (parse_has_error(ctx)) {
      fclose(f);
      return false;
    }
  }

  fclose(f);
  return true;
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
    process_line(ctx, line, &should_exit);
    return parse_has_error(ctx) ? 1 : 0;
  }

  char line[1024];
  bool interactive = isatty(STDIN_FILENO);
  bool should_exit = false;

  while (!should_exit) {
    if (interactive) {
      printf("> ");
      fflush(stdout);
    }

    if (!fgets(line, sizeof(line), stdin))
      break;
    process_line(ctx, line, &should_exit);
  }

  return 0;
}