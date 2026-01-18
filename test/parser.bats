#!/usr/bin/env bats

setup() {
    PROLOG="./prolog"
}


# --- valid terms ---

@test "parse: atom" {
    run bash -c "echo '?- foo' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]  # no clauses, but parsed ok
}

@test "parse: number" {
    run bash -c "echo -e 'num(42).\n?- num(42)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "parse: negative number" {
    run bash -c "echo -e 'num(-5).\n?- num(-5)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "parse: variable" {
    run bash -c "echo -e 'foo(a).\n?- foo(X)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = a"* ]]
}

@test "parse: underscore variable" {
    run bash -c "echo -e 'foo(a, b).\n?- foo(_, X)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = b"* ]]
}

@test "parse: functor no args" {
    run bash -c "echo -e 'foo.\n?- foo' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "parse: functor one arg" {
    run bash -c "echo -e 'foo(a).\n?- foo(a)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "parse: functor multiple args" {
    run bash -c "echo -e 'foo(a, b, c).\n?- foo(a, b, c)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "parse: nested functors" {
    run bash -c "echo -e 'foo(bar(baz(x))).\n?- foo(bar(baz(X)))' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = x"* ]]
}

# --- lists ---

@test "parse: empty list" {
    run bash -c "echo -e 'foo([]).\n?- foo([])' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "parse: singleton list" {
    run bash -c "echo -e 'foo([a]).\n?- foo([X])' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = a"* ]]
}

@test "parse: multi-element list" {
    run bash -c "echo -e 'foo([a,b,c]).\n?- foo([a,b,c])' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "parse: head|tail pattern" {
    run bash -c "echo -e 'foo([1,2,3]).\n?- foo([H|T])' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"H = 1"* ]]
    [[ "$output" == *"T = [2, 3]"* ]]
}

@test "parse: head|tail empty tail" {
    run bash -c "echo -e 'foo([1]).\n?- foo([H|T])' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"H = 1"* ]]
    [[ "$output" == *"T = []"* ]]
}

@test "parse: multiple heads" {
    run bash -c "echo -e 'foo([1,2,3]).\n?- foo([A,B|T])' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"A = 1"* ]]
    [[ "$output" == *"B = 2"* ]]
    [[ "$output" == *"T = [3]"* ]]
}

@test "parse: nested lists" {
    run bash -c "echo -e 'foo([[a,b],[c]]).\n?- foo([[a,X],Y])' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = b"* ]]
    [[ "$output" == *"Y = [c]"* ]]
}

# --- clauses ---

@test "parse: fact" {
    run bash -c "echo -e 'likes(mary, food).\n?- likes(mary, food)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "parse: rule single body" {
    run bash -c "echo -e 'a(x).\nb(X) :- a(X).\n?- b(X)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = x"* ]]
}

@test "parse: rule multiple body goals" {
    run bash -c "echo -e 'a(x).\nb(x).\nc(X) :- a(X), b(X).\n?- c(X)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = x"* ]]
}

# --- whitespace handling ---

@test "parse: extra spaces" {
    run bash -c "echo -e 'foo(  a  ,  b  ).\n?- foo( a , b )' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "parse: tabs" {
    run bash -c "printf 'foo(\ta\t).\n?- foo(X)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = a"* ]]
}

@test "parse: no spaces around :-" {
    run bash -c "echo -e 'a(x).\nb(X):-a(X).\n?- b(X)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = x"* ]]
}

# --- query syntax ---

@test "parse: query single goal" {
    run bash -c "echo -e 'foo(a).\n?- foo(X)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = a"* ]]
}

@test "parse: query multiple goals" {
    run bash -c "echo -e 'foo(a).\nbar(a).\n?- foo(X), bar(X)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = a"* ]]
}

# --- edge cases ---

@test "parse: atom with underscore" {
    run bash -c "echo -e 'foo_bar(x).\n?- foo_bar(X)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = x"* ]]
}

@test "parse: atom with numbers" {
    run bash -c "echo -e 'foo123(x).\n?- foo123(X)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = x"* ]]
}

@test "parse: variable with underscore" {
    run bash -c "echo -e 'foo(a).\n?- foo(X_1)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X_1 = a"* ]]
}

@test "parse: empty input" {
    run bash -c "echo '' | $PROLOG"
    [ "$status" -eq 0 ]
}

@test "parse: only whitespace" {
    run bash -c "echo '   ' | $PROLOG"
    [ "$status" -eq 0 ]
}


# --- syntax errors ---

@test "error: unclosed parenthesis" {
    run bash -c "echo '?- foo(a' | $PROLOG 2>&1"
    [ "$status" -eq 0 ]
    [[ "$output" == *"expected ')'"* ]]
}

@test "error: unclosed list" {
    run bash -c "echo '?- foo([a,b' | $PROLOG 2>&1"
    [ "$status" -eq 0 ]
    [[ "$output" == *"expected ']'"* ]]
}

@test "error: unexpected character" {
    skip
    run bash -c "echo '?- @foo' | $PROLOG 2>&1"
    [ "$status" -eq 0 ]
    [[ "$output" == *"unexpected character"* ]]
}

@test "error: missing list tail after |" {
    skip
    run bash -c "echo '?- foo([a|])' | $PROLOG 2>&1"
    [ "$status" -eq 0 ]
    [[ "$output" == *"unexpected character"* ]] || [[ "$output" == *"expected"* ]]
}

@test "error: empty query" {
    run bash -c "echo '?-' | $PROLOG 2>&1"
    [ "$status" -eq 0 ]
    [[ "$output" == *"error"* ]] || [[ "$output" == *"Error"* ]]
}

@test "error: invalid file" {
    run $PROLOG -f nonexistent.pl 2>&1
    [ "$status" -eq 1 ]
    [[ "$output" == *"cannot open file"* ]]
}

@test "error: caret points to error location" {
    run bash -c "echo '?- foo(a, @bad)' | $PROLOG 2>&1"
    [ "$status" -eq 0 ]
    [[ "$output" == *"@bad"* ]]
    [[ "$output" == *"^"* ]]
    [[ "$output" == *"error:"* ]]
}

# --- recovery ---

@test "error recovery: continue after error in interactive" {
    run bash -c "printf '?- @bad\nfoo(a).\n?- foo(X).\n' | $PROLOG 2>&1"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = a"* ]]
}
