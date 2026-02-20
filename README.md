A lightweight, embeddable Prolog interpreter written in C11.

## Table of Contents

- [Language](#language)
- [Build](#build)
- [Algorithm](#algorithm)

## Language

**Syntax elements**

| Element | Syntax | Example |
|---------|--------|---------|
| Atom | lowercase identifier or quoted | `foo`, `'hello world'` |
| Variable | uppercase or `_` prefix | `X`, `_`, `_Tail` |
| Integer | decimal digits | `0`, `42`, `-1` |
| String | double-quoted | `"hello"` |
| Functor | `name(arg, ...)` | `f(a, B)` |
| List | `[head\|tail]` or `[]` | `[1,2,3]`, `[H\|T]` |
| Fact | `head.` | `likes(alice, bob).` |
| Rule | `head :- body.` | `mortal(X) :- human(X).` |
| Query | `?- goal.` | `?- member(X, [1,2]).` |
| Comment | `%` to end of line | `% this is a comment` |

**Infix operators** (parsed with precedence)

| Operator | Precedence | Description |
|----------|-----------|-------------|
| `*` `/` `mod` | 40 | multiply, divide, modulo |
| `+` `-` | 30 | add, subtract |
| `<` `>` `=<` `>=` `=:=` `=\=` | 20 | arithmetic comparison |
| `is` `=` `\=` | 10 | evaluate, unify, not-unify |

**Built-in predicates**

| Predicate | Description |
|-----------|-------------|
| `true` | always succeeds |
| `fail` | always fails |
| `!` | cut — discard choice points back to the parent |
| `X is Expr` | evaluate arithmetic expression, unify with `X` |
| `X = Y` | unify `X` and `Y` |
| `X \= Y` | succeed if `X` and `Y` do not unify |
| `X < Y` | arithmetic less-than |
| `X > Y` | arithmetic greater-than |
| `X =< Y` | arithmetic less-or-equal |
| `X >= Y` | arithmetic greater-or-equal |
| `X =:= Y` | arithmetic equal |
| `X =\= Y` | arithmetic not-equal |
| `nl` | print a newline |
| `write(Term)` | print `Term` |
| `writeln(Term)` | print `Term` followed by a newline |
| `findall(T, Goal, List)` | collect all `T` for which `Goal` succeeds into `List` |
| `bagof(T, Goal, List)` | like `findall` but fails if there are no solutions |
| `include(File)` | load and assert clauses from `File` |
| `stats` | print unification/backtrack/allocation counts |

**Arithmetic expressions** (usable inside `is/2` and comparison operators)

| Expression | Description |
|------------|-------------|
| `N` | integer literal |
| `X + Y` | addition |
| `X - Y` | subtraction |
| `X * Y` | multiplication |
| `X / Y` | integer division |
| `X mod Y` | modulo |

## Build

```sh
make        # build interpreter
make test   # run BATS test suite
```

## Algorithm

The interpreter is a direct implementation of the **ABC algorithm** described in M.H. van Emden's *"An Algorithm for Interpreting PROLOG Programs"* (University of Waterloo, CS-81-28, 1981).

The ABC algorithm is a depth-first, left-to-right tree search expressed as three labeled program points:

```
initialize stack; cn := initial goal statement
A: if cn is the empty goal statement
       then halt with success
       else initialize son() for cn; goto B
B: if son(cn, x)       -- x is the next resolvent of cn
       then push cn; cn := x; goto A
       else goto C     -- all clauses exhausted, backtrack
C: if stack nonempty
       then pop stack into cn; goto B
       else halt with failure
```

`son(cn, x)` is the clause generator: it iterates over program clauses whose head unifies with the selected (leftmost) goal of `cn`, and on each successful unification returns the new resolvent `x`. This directly maps to SLD resolution over the search tree rooted at the initial goal.

The stack frames store the proof tree path. Each frame records the current goal, the clause generator state (for backtracking), the environment mark (for undoing bindings), and the cut point. Variable renaming uses `name#id` scoping so each clause invocation gets fresh variables.
