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
| String | double-quoted; behaves as a list of single-char atoms | `"hello"` |
| Functor | `name(arg, ...)` | `f(a, B)` |
| List | `[head\|tail]` or `[]` | `[1,2,3]`, `[H\|T]` |
| Fact | `head.` | `likes(alice, bob).` |
| Rule | `head :- body.` | `mortal(X) :- human(X).` |
| Query | `?- goal.` | `?- member(X, [1,2]).` |
| Comment | `%` to end of line | `% this is a comment` |

**Strings as character lists**

Double-quoted strings unify with list patterns, so all list predicates work on strings directly:

```prolog
?- [L|Ls] = "abc".          % L = a, Ls = "bc"
?- [H|_] = "hello".         % H = h
?- "abc" = [a,b,c].         % true
?- length("hello", N).      % N = 5
?- member(X, "abc").        % X = a
?- reverse("abc", X).       % X = [c, b, a]
```

A string printed as the tail of a partial list keeps its quoted form:

```prolog
?- [A,B|Rest] = "prolog".   % A = p, B = r, Rest = "olog"
```

The empty string `""` unifies with the empty list `[]`.

`write/1` prints strings as raw bytes (no quotes or escapes); `writeq/1` prints them in quoted form that can be read back:

```prolog
?- write("hello\n").    % prints: hello<newline>
?- writeq("hello\n").   % prints: "hello\n"
```

**Infix operators** (parsed with precedence)

| Operator | Precedence | Description |
|----------|-----------|-------------|
| `*` `/` `//` `mod` | 40 | multiply, divide, integer divide, modulo |
| `+` `-` | 30 | add, subtract |
| `<` `>` `=<` `>=` `=:=` `=\=` | 20 | arithmetic comparison |
| `is` `=` `\=` `=..` | 10 | evaluate, unify, not-unify, univ |

**Built-in predicates**

*Control*

| Predicate | Description |
|-----------|-------------|
| `true` | always succeeds |
| `fail` | always fails |
| `!` | cut — discard choice points back to the parent |
| `\+ Goal` | negation as failure — succeeds if `Goal` fails |
| `call(Goal)` | call `Goal`; supports backtracking |
| `once(Goal)` | call `Goal`, cut after first solution |
| `findall(T, Goal, List)` | collect all `T` for which `Goal` succeeds into `List` |
| `bagof(T, Goal, List)` | like `findall` but fails if there are no solutions |
| `include(File)` | load and assert clauses from `File` |

*Unification and arithmetic*

| Predicate | Description |
|-----------|-------------|
| `X is Expr` | evaluate arithmetic expression, unify with `X` |
| `X = Y` | unify `X` and `Y` |
| `X \= Y` | succeed if `X` and `Y` do not unify |
| `X < Y` | arithmetic less-than |
| `X > Y` | arithmetic greater-than |
| `X =< Y` | arithmetic less-or-equal |
| `X >= Y` | arithmetic greater-or-equal |
| `X =:= Y` | arithmetic equal |
| `X =\= Y` | arithmetic not-equal |
| `succ(X, Y)` | `Y = X + 1`; bidirectional |
| `plus(X, Y, Z)` | `Z = X + Y`; solves for any one unknown |

*Type tests*

| Predicate | Description |
|-----------|-------------|
| `var(X)` | unbound variable |
| `nonvar(X)` | not an unbound variable |
| `atom(X)` | non-numeric atom |
| `integer(X)` | integer |
| `number(X)` | integer (alias) |
| `atomic(X)` | atom or string |
| `compound(X)` | functor with arity ≥ 1 |
| `callable(X)` | atom or compound |
| `string(X)` | double-quoted string literal |
| `is_list(X)` | proper list |

*Atom and string manipulation*

| Predicate | Description |
|-----------|-------------|
| `atom_length(Atom, N)` | `N` = length of `Atom` |
| `atom_concat(A, B, C)` | `C = A ++ B`; any one arg may be unbound |
| `atom_chars(Atom, Chars)` | convert between atom and list of single-char atoms |
| `atom_codes(Atom, Codes)` | convert between atom and list of character codes |
| `atom_number(Atom, N)` | convert between atom representation and integer |
| `char_code(Char, Code)` | convert single-char atom to/from ASCII code |
| `number_codes(N, Codes)` | like `atom_codes` but validates `N` is an integer |
| `number_chars(N, Chars)` | like `atom_chars` but validates `N` is an integer |

*Term introspection*

| Predicate | Description |
|-----------|-------------|
| `functor(Term, Name, Arity)` | decompose or construct a term |
| `arg(N, Term, Arg)` | `Arg` = Nth argument of `Term` (1-indexed) |
| `Term =.. List` | univ — `foo(a,b) =.. [foo,a,b]`; bidirectional |
| `copy_term(Original, Copy)` | deep copy with fresh variables |

*Dynamic database*

| Predicate | Description |
|-----------|-------------|
| `assertz(Clause)` | add `Clause` at end of database |
| `asserta(Clause)` | add `Clause` at start of database |
| `retract(Head)` | remove first clause matching `Head` |
| `retractall(Head)` | remove all clauses matching `Head`; always succeeds |
| `make` | reload all files previously loaded with `include/1`; facts asserted before the first include are preserved |

*I/O*

| Predicate | Description |
|-----------|-------------|
| `nl` | print a newline |
| `write(Term)` | print `Term`; strings are printed as raw bytes (no quotes) |
| `writeln(Term)` | print `Term` followed by a newline |
| `writeq(Term)` | print `Term` in quoted form; strings printed as `"..."` with escape sequences |
| `stats` | print unification/backtrack/allocation counts |

**Arithmetic expressions** (usable inside `is/2` and comparison operators)

| Expression | Description |
|------------|-------------|
| `N` | integer literal |
| `X + Y` | addition |
| `X - Y` | subtraction |
| `-X` | unary negation |
| `X * Y` | multiplication |
| `X / Y` | integer division |
| `X // Y` | integer division (explicit) |
| `X mod Y` | modulo |
| `abs(X)` | absolute value |
| `max(X, Y)` | maximum |
| `min(X, Y)` | minimum |

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
