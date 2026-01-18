#!/usr/bin/env bats

setup() {
    PROLOG="./prolog"
}

# --- is/2 basic arithmetic ---

@test "is: simple number" {
    run bash -c "echo '?- X is 5' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = 5"* ]]
}

@test "is: addition" {
    run bash -c "echo '?- X is 2 + 3' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = 5"* ]]
}

@test "is: subtraction" {
    run bash -c "echo '?- X is 10 - 4' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = 6"* ]]
}

@test "is: multiplication" {
    run bash -c "echo '?- X is 3 * 4' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = 12"* ]]
}

@test "is: division" {
    run bash -c "echo '?- X is 15 / 3' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = 5"* ]]
}

@test "is: integer division truncates" {
    run bash -c "echo '?- X is 7 / 2' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = 3"* ]]
}

@test "is: modulo" {
    run bash -c "echo '?- X is 17 mod 5' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = 2"* ]]
}

@test "is: modulo zero remainder" {
    run bash -c "echo '?- X is 10 mod 5' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = 0"* ]]
}

@test "is: negative numbers" {
    run bash -c "echo '?- X is -3 + 5' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = 2"* ]]
}

@test "is: result is negative" {
    run bash -c "echo '?- X is 3 - 10' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = -7"* ]]
}

# --- is/2 complex expressions ---

@test "is: nested addition" {
    run bash -c "echo '?- X is 1 + 2 + 3' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = 6"* ]]
}

@test "is: precedence mul over add" {
    run bash -c "echo '?- X is 2 + 3 * 4' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = 14"* ]]
}

@test "is: precedence with subtraction" {
    run bash -c "echo '?- X is 10 - 2 * 3' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = 4"* ]]
}

@test "is: parentheses override precedence" {
    run bash -c "echo '?- X is (2 + 3) * 4' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = 20"* ]]
}

@test "is: multiple operations" {
    run bash -c "echo '?- X is 2 * 3 + 4 * 5' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = 26"* ]]
}

@test "is: division and multiplication" {
    run bash -c "echo '?- X is 20 / 4 * 2' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = 10"* ]]
}

# --- is/2 with variables ---

@test "is: variable in expression" {
    run bash -c "echo -e 'num(5).\n?- num(N), X is N + 1' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = 6"* ]]
}

@test "is: two variables" {
    run bash -c "echo -e 'pair(3, 4).\n?- pair(A, B), X is A + B' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = 7"* ]]
}

@test "is: unify with known value succeeds" {
    run bash -c "echo '?- 5 is 2 + 3' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "is: unify with wrong value fails" {
    run bash -c "echo '?- 6 is 2 + 3' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

# --- is/2 failures ---

@test "is: unbound variable in expression fails" {
    run bash -c "echo '?- X is Y + 1' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

@test "is: non-numeric atom fails" {
    run bash -c "echo '?- X is foo + 1' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

# --- comparison: less than ---

@test "less than: true" {
    run bash -c "echo '?- 3 < 5' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "less than: false when equal" {
    run bash -c "echo '?- 5 < 5' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

@test "less than: false when greater" {
    run bash -c "echo '?- 7 < 5' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

@test "less than: with expressions" {
    run bash -c "echo '?- 2 + 1 < 2 * 2' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

# --- comparison: greater than ---

@test "greater than: true" {
    run bash -c "echo '?- 5 > 3' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "greater than: false when equal" {
    run bash -c "echo '?- 5 > 5' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

@test "greater than: false when less" {
    run bash -c "echo '?- 3 > 5' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

@test "greater than: with expressions" {
    run bash -c "echo '?- 3 * 3 > 2 + 5' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

# --- comparison: less than or equal ---

@test "less equal: true when less" {
    run bash -c "echo '?- 3 =< 5' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "less equal: true when equal" {
    run bash -c "echo '?- 5 =< 5' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "less equal: false when greater" {
    run bash -c "echo '?- 7 =< 5' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

# --- comparison: greater than or equal ---

@test "greater equal: true when greater" {
    run bash -c "echo '?- 5 >= 3' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "greater equal: true when equal" {
    run bash -c "echo '?- 5 >= 5' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "greater equal: false when less" {
    run bash -c "echo '?- 3 >= 5' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

# --- comparison: arithmetic equal ---

@test "arith equal: same numbers" {
    run bash -c "echo '?- 5 =:= 5' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "arith equal: equal expressions" {
    run bash -c "echo '?- 2 + 3 =:= 1 + 4' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "arith equal: different values" {
    run bash -c "echo '?- 5 =:= 6' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

# --- comparison: arithmetic not equal ---

@test "arith not equal: different numbers" {
    run bash -c "echo '?- 5 =\\= 6' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "arith not equal: same numbers" {
    run bash -c "echo '?- 5 =\\= 5' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

@test "arith not equal: expressions" {
    run bash -c "echo '?- 2 * 3 =\\= 2 + 3' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

# --- true/0 and fail/0 ---

@test "true: succeeds" {
    run bash -c "echo '?- true' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "fail: fails" {
    run bash -c "echo '?- fail' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

@test "true in conjunction" {
    run bash -c "echo -e 'foo(a).\n?- foo(X), true' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = a"* ]]
}

@test "fail in conjunction" {
    run bash -c "echo -e 'foo(a).\n?- foo(X), fail' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

# --- unification: =/2 ---

@test "unify: same atoms" {
    run bash -c "echo '?- a = a' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "unify: different atoms" {
    run bash -c "echo '?- a = b' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

@test "unify: variable to atom" {
    run bash -c "echo '?- X = foo' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = foo"* ]]
}

@test "unify: two variables" {
    run bash -c "echo '?- X = Y, X = hello' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"hello"* ]]
}

@test "unify: structures" {
    run bash -c "echo '?- foo(a, B) = foo(A, b)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"A = a"* ]]
    [[ "$output" == *"B = b"* ]]
}

@test "unify: structures different functors" {
    run bash -c "echo '?- foo(a) = bar(a)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

@test "unify: structures different arity" {
    run bash -c "echo '?- foo(a) = foo(a, b)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

@test "unify: lists" {
    run bash -c "echo '?- [H|T] = [1, 2, 3]' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"H = 1"* ]]
    [[ "$output" == *"T = [2, 3]"* ]]
}

# --- not unifiable: \=/2 ---

@test "not unify: different atoms" {
    run bash -c "echo '?- a \\= b' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "not unify: same atoms" {
    run bash -c "echo '?- a \\= a' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

@test "not unify: variable and atom" {
    run bash -c "echo '?- X \\= a' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

@test "not unify: different structures" {
    run bash -c "echo '?- foo(a) \\= foo(b)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "not unify: does not bind variables" {
    run bash -c "echo '?- X \\= a, X = b' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]] || [[ "$output" == *"X = b"* ]]
}

# --- combined builtins ---

@test "combined: arithmetic in rule" {
    run bash -c "echo -e 'double(X, Y) :- Y is X * 2.\n?- double(5, D)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"D = 10"* ]]
}

@test "combined: comparison guard" {
    run bash -c "echo -e 'positive(X) :- X > 0.\n?- positive(5)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "combined: comparison guard fails" {
    run bash -c "echo -e 'positive(X) :- X > 0.\n?- positive(-3)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

@test "combined: max of two" {
    run bash -c "echo -e 'max(X, Y, X) :- X >= Y.\nmax(X, Y, Y) :- Y > X.\n?- max(3, 7, M)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"M = 7"* ]]
}

@test "combined: factorial base case" {
    run bash -c "echo -e 'fact(0, 1).\nfact(N, F) :- N > 0, N1 is N - 1, fact(N1, F1), F is N * F1.\n?- fact(0, F)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"F = 1"* ]]
}

@test "combined: factorial of 5" {
    run bash -c "echo -e 'fact(0, 1).\nfact(N, F) :- N > 0, N1 is N - 1, fact(N1, F1), F is N * F1.\n?- fact(5, F)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"F = 120"* ]]
}

@test "combined: sum list" {
    run bash -c "echo -e 'sum([], 0).\nsum([H|T], S) :- sum(T, S1), S is S1 + H.\n?- sum([1,2,3,4], S)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"S = 10"* ]]
}

# --- edge cases ---

@test "edge: zero operations" {
    run bash -c "echo '?- X is 0 + 0' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = 0"* ]]
}

@test "edge: multiply by zero" {
    run bash -c "echo '?- X is 5 * 0' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = 0"* ]]
}

@test "edge: comparison with zero" {
    run bash -c "echo '?- 0 < 1' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "edge: negative comparison" {
    run bash -c "echo '?- -5 < -3' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}