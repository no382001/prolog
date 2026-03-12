A lightweight, embeddable Prolog interpreter written in C11.

## Build

```sh
make
```

## Usage

```sh
./prolog                  # interactive REPL
./prolog -f file.pl       # load file
./prolog -e "goal."       # evaluate and exit
./prolog -q tests.pl      # run quad tests
```

## Language

Standard Prolog syntax. Integers, atoms, functors, lists, rules and facts.

**Operators**

| Operators | Prec | Notes |
|-----------|------|-------|
| `* / // mod` | 40 | |
| `+ -` | 30 | |
| `< > =< >= =:= =\= == \== @< @> @=< @>=` | 20 | |
| `is = \= =..` | 10 | |
| `->` | 7 | if-then |
| `;` | 5 | disjunction / if-then-else |

**Built-ins**

| | |
|-|-|
| `true` `fail` `!` | basics |
| `\+(G)` `call(G)` | meta-call |
| `,(G,G)` `;(G,G)` `->(G,G)` | control |
| `throw(T)` `catch(G,C,R)` | exceptions (`error(Formal, Context)` convention) |
| `findall/3` `bagof/3` | aggregation |
| `consult(F)` `include(F)` `make` | loading |
| `assertz(C)` `asserta(C)` `retract(H)` `retractall(H)` | database |
| `var/nonvar/atom/integer/number/atomic/compound/callable/string/is_list` | type tests |
| `float(X)` | always fails (no float type) |
| `functor/3` `arg/3` `=../2` `copy_term/2` | introspection |
| `is/2` `succ/2` `plus/3` | arithmetic |
| `compare/3` | standard order |
| `sort/2` `msort/2` | sorting |
| `atom_length/2` `atom_concat/3` `atom_chars/2` `atom_codes/2` | atoms |
| `char_code/2` `atom_number/2` `number_chars/2` `number_codes/2` | conversion |
| `write/1` `writeln/1` `writeq/1` `nl` | output |

**Standard library (`core.pl`, loaded automatically)**

`once/1`, `between/3`, `forall/2`, `member/2`, `append/3`, `length/2`, `reverse/2`, `last/2`

## Embedding

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

### Freestanding (no libc)

Define `PROLOG_FREESTANDING` before including `prolog.h`. The header then expects no standard includes — you provide the required primitives as macros:

```c
#define PROLOG_FREESTANDING
#define strcmp   my_strcmp
#define strlen   my_strlen
#define memcpy   my_memcpy
#define vsnprintf my_vsnprintf
// ... etc.
#include "prolog.h"
```

All output must be handled through I/O hooks; there is no fallback to `printf`. See `examples/freestanding.c` for a build smoke-test.

```sh
make freestanding   # verifies the library links without libc
```

## Testing

Tests use the [quad format](https://web.liminal.cafe/~byakuren/flowlog/docs/QUAD_TESTS.html) — plain `.pl` files containing queries and their expected output:

```prolog
?- member(X, [a,b,c]).
   X = a.
   X = b.
   X = c.
```

```sh
make quad          # TAP output
make quad-junit    # JUnit XML → _build/test-results/
```

ISO 13211-1 conformance tests (`test/iso_quad.pl`) run but do not fail the build.

## Algorithm

Based on the **ABC algorithm** from M.H. van Emden's *"An Algorithm for Interpreting PROLOG Programs"* (1981): a depth-first, left-to-right SLD resolution loop with an explicit stack for backtracking. Each clause invocation gets fresh variables via an integer counter.
