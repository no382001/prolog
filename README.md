# trilog

A Prolog interpreter aiming to be embeddable, based on van Emden's ABC algorithm (hence the three/tri), written in C11.

## Contents

- [Build](#build)
- [Usage](#usage)
- [Language](#language)
  - [Operators](#operators)
  - [Arithmetic](#arithmetic)
  - [ISO built-ins](#iso-built-ins)
  - [Extensions](#extensions)
  - [Standard library](#standard-library-libcorepl)
- [Embedding](#embedding)
  - [Context allocation](#context-allocation)
  - [Custom builtins (FFI)](#custom-builtins-ffi)
  - [I/O hooks](#io-hooks)
- [Testing](#testing)
- [Algorithm](#algorithm)

## Build

```sh
make
```

## Usage

```sh
./trilog                  # interactive REPL
./trilog file.pl          # load file
./trilog -f file.pl       # load file, skip ~/.trilog (fast startup)
./trilog -e "goal."       # evaluate and exit
```

On startup, trilog loads `~/.trilog` if it exists, unless `-f` (fast startup) is given.

## Language

Standard Prolog syntax. Integers, atoms, functors, lists, rules and facts. Comments: `%` line comments and `/* */` block comments. Character code notation: `0'a` evaluates to 97.

### Operators

| Operators | Prec | Notes |
|-----------|------|-------|
| `* / // mod >> << /\ xor` | 40 | `>>` `<<` shift; `/\` bitwise and |
| `+ - \/` | 30 | `\/` bitwise or |
| `< > =< >= =:= =\= == \== @< @> @=< @>=` | 20 | |
| `is = \= =..` | 10 | |
| `->` | 7 | if-then |
| `^` | 6 | existential quantification |
| `;` | 5 | disjunction / if-then-else |

Prefix: `\+` (negation), `\` (bitwise complement).

`op/3` and `current_op/3` are supported for defining and querying operators at runtime.

### Arithmetic

Integer arithmetic via `is/2`. Operators: `+ - * / // mod max min >> << /\ \/ xor`. Unary: `- abs \`. ISO overflow and zero-divisor errors are raised.

### ISO built-ins

| Predicate | Notes |
|-----------|-------|
| `true` `fail` `!` `halt/0` `halt/1` | basics |
| `\+(G)` `call(G)` `once(G)` | meta-call |
| `,(G,G)` `;(G,G)` `->(G,G)` | control |
| `throw(T)` `catch(G,C,R)` | exceptions (`error(Formal, Context)` convention) |
| `findall/3` `bagof/3` `setof/3` | aggregation; `X^Goal` for existential quantification |
| `asserta(C)` `assertz(C)` `retract(H)` `retractall(H)` `abolish(F/A)` | dynamic database |
| `dynamic(+Spec)` | declares predicate as dynamic; file-loaded predicates without a `dynamic` declaration are protected from modification |
| `var/1` `nonvar/1` `atom/1` `integer/1` `number/1` `atomic/1` `compound/1` `callable/1` `is_list/1` | type tests |
| `functor/3` `arg/3` `=../2` `copy_term/2` | term introspection |
| `compare/3` `sort/2` | ordering |
| `atom_length/2` `atom_concat/3` `atom_chars/2` `atom_codes/2` `sub_atom/5` | atoms |
| `char_code/2` `atom_number/2` `number_chars/2` `number_codes/2` | conversion |
| `write/1` `write/2` `writeq/1` `writeq/2` `nl/0` `nl/1` `flush_output/0` `get_char/1` | I/O; `/2` variants take a stream as first arg |
| `open/3` `close/1` `read_term/2` | streams |
| `current_prolog_flag/2` | flags: `max_integer` `min_integer` `bounded` `integer_rounding_function` |
| `is/2` | arithmetic |

### Extensions

These are non-ISO predicates

| Predicate | Notes |
|-----------|-------|
| `consult(+F)` `[F]` `[F1,F2,…]` `include(+F)` `make` | file loading; list syntax consults each element |
| `consulted(-Ls)` | unifies `Ls` with the list of currently loaded files |
| `unconsult(+F)` | unloads all clauses contributed by file `F`; fails if `F` is not loaded |
| `msort/2` | sort without removing duplicates |
| `writeln/1` `writeln/2` `with_output_to(+Sink, +Goal)` | output; Sink: `atom(A)`, `codes(Cs)`, `chars(Chs)` |
| `read_line_to_atom/2` `read_line_to_chars/2` | read one line from a stream; unifies `end_of_file` at EOF |
| `put_chars/1` `put_chars/2` | write a char list to stdout / stream |
| `read_from_chars/2` `read_term_from_chars/3` | read a term from a char list |
| `write_term_to_chars/3` | write a term to a char list with options |
| `atom_to_term/3` `term_to_atom/2` | term <-> atom |

### Standard library (`lib/core.pl`)

Loaded automatically. Provides: `false/0`, `repeat/0`, `halt/0`, `succ/2`, `plus/3`, `between/3`, `forall/2`, `member/2`, `memberchk/2`, `select/3`, `append/3`, `length/2`, `reverse/2`, `last/2`, `nth0/3`, `nth1/3`, `numlist/3`, `permutation/2`, `delete/3`, `subtract/3`, `intersection/3`, `union/3`, `list_to_set/2`, `flatten/2`, `sum_list/2`, `max_list/2`, `min_list/2`, `max_member/2`, `min_member/2`, `include/3`, `exclude/3`, `partition/4`, `maplist/2-4`, `foldl/4-6`, `countall/3`, `must_be/2`.

## Embedding

### Context allocation

The library itself performs no dynamic allocation — the host allocates a single contiguous block for the interpreter context (including its term pool) and passes it in:

```c
trilog_ctx_t *ctx = malloc(TRILOG_CTX_SIZE(TERM_POOL_BYTES));
trilog_ctx_init(ctx, TERM_POOL_BYTES);
io_hooks_init_default(ctx);
// ... use ctx ...
free(ctx);
```

`TERM_POOL_BYTES` defaults to 256 MB. Override at compile time for constrained targets (e.g. `-DTERM_POOL_BYTES=(128*1024)` for RP2040).

### Custom builtins (FFI)

```c
builtin_result_t my_handler(trilog_ctx_t *ctx, term_t *goal, env_t *env) {
    term_t *arg = deref(env, goal->args[0]);
    // ... inspect/unify args ...
    return BUILTIN_OK; // or BUILTIN_FAIL / BUILTIN_ERROR
}
ffi_register_builtin(ctx, "my_pred", 1, my_handler, NULL);
```

See `examples/ffi_example.c` for a complete example.

### I/O hooks

All interpreter I/O goes through a hook struct, so you can redirect it entirely — useful for embedding in applications, GUIs, or constrained environments.

```c
io_hooks_t hooks = {0};
hooks.write_str  = my_write;    // void(ctx, str, userdata)
hooks.writef     = my_writef;   // void(ctx, fmt, va_list, userdata)
hooks.writef_err = my_writef;   // stderr channel
hooks.read_char  = my_getchar;  // int(ctx, userdata)
hooks.read_line  = my_readline; // char*(ctx, buf, size, userdata)
// file i/o (needed for consult/include):
hooks.file_open      = my_fopen;
hooks.file_close     = my_fclose;
hooks.file_read_line = my_freadline;
hooks.file_write     = my_fwrite;
hooks.file_exists    = my_exists;
hooks.file_mtime     = my_mtime;
hooks.clock_monotonic = my_clock; // for test timing
hooks.userdata = my_state;
io_hooks_set(ctx, &hooks);
```

Only set the callbacks you need; unset ones fall back to the defaults (`stdio`/`libc`).

## Testing

Tests use the [quad format](https://web.liminal.cafe/~byakuren/flowlog/docs/QUAD_TESTS.html) — plain `.pl` files containing queries and their expected output:

```prolog
?- member(X, "abc").
   X = a
;  X = b
;  X = c.

?- atom_length(hello, N).
   N = 5.
```

```sh
make quad          # TAP output via quad format
make quad-junit    # JUnit XML → _build/test-results/
make syscheck      # bats system tests (test/*.bats)
make test          # quad + syscheck
```

There are ISO conformance tests (`test/iso_quad.pl`) that run but do not fail the build.

## Algorithm

Based on the **ABC algorithm** from M.H. van Emden's *"An Algorithm for Interpreting PROLOG Programs"* (1981): a depth-first, left-to-right SLD resolution loop with an explicit stack for backtracking. Each clause invocation gets fresh variables via an integer counter.

**Memory layout** — dual-ended bump allocator inside the context buffer: temporary query terms grow up from offset 0; permanent clause terms grow down from the top. After each top-level query the temp region is reclaimed unconditionally. A staging-area compaction pass (`compact_perm_pool`) defragments the permanent region after retracts, controlled by the `COMPACT_AFTER_RETRACTS` macro (default: compact every retract).
