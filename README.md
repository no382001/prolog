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

Standard Prolog syntax: atoms, variables (`X`, `_`), integers, functors, lists, rules and facts.

Strings are double-quoted and behave as lists of single-char atoms: `"abc" = [a,b,c]`.

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
| `throw(T)` `catch(G,C,R)` | exceptions |
| `findall/3` `bagof/3` | aggregation |
| `consult(F)` `include(F)` `make` | loading |
| `assert(z/a)(C)` `retract(H)` `retractall(H)` | database |
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

**Exceptions**

Error terms follow the ISO convention `error(Formal, Context)`. Arithmetic and type errors are thrown automatically; use `throw/1` and `catch/3` for user-defined exceptions.

## Testing

Tests are written in the [quad format](https://web.liminal.cafe/~byakuren/flowlog/docs/QUAD_TESTS.html) — plain `.pl` files containing queries and their expected output:

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

## Algorithm

Based on the **ABC algorithm** from M.H. van Emden's *"An Algorithm for Interpreting PROLOG Programs"* (1981): a depth-first, left-to-right SLD resolution loop with an explicit stack for backtracking. Each clause invocation gets fresh variables via an integer counter.
