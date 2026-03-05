#!/usr/bin/env bats

setup() {
    PROLOG="./prolog"
    tmpfile=$(mktemp /tmp/prolog_test_XXXXXX.pl)
}

teardown() {
    rm -f "$tmpfile"
}


# --- valid terms ---

@test "parse: atom" {
    run bash -c "echo '?- foo' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]  # no clauses, but parsed ok
}

@test "parse: number" {
    printf 'num(42).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- num(42)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "parse: negative number" {
    printf 'num(-5).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- num(-5)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "parse: variable" {
    printf 'foo(a).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- foo(X)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = a"* ]]
}

@test "parse: underscore variable" {
    printf 'foo(a, b).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- foo(_, X)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = b"* ]]
}

@test "parse: functor no args" {
    printf 'foo.\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- foo'
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "parse: functor one arg" {
    printf 'foo(a).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- foo(a)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "parse: functor multiple args" {
    printf 'foo(a, b, c).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- foo(a, b, c)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "parse: nested functors" {
    printf 'foo(bar(baz(x))).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- foo(bar(baz(X)))'
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = x"* ]]
}

# --- lists ---

@test "parse: empty list" {
    printf 'foo([]).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- foo([])'
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "parse: singleton list" {
    printf 'foo([a]).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- foo([X])'
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = a"* ]]
}

@test "parse: multi-element list" {
    printf 'foo([a,b,c]).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- foo([a,b,c])'
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "parse: head|tail pattern" {
    printf 'foo([1,2,3]).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- foo([H|T])'
    [ "$status" -eq 0 ]
    [[ "$output" == *"H = 1"* ]]
    [[ "$output" == *"T = [2, 3]"* ]]
}

@test "parse: head|tail empty tail" {
    printf 'foo([1]).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- foo([H|T])'
    [ "$status" -eq 0 ]
    [[ "$output" == *"H = 1"* ]]
    [[ "$output" == *"T = []"* ]]
}

@test "parse: multiple heads" {
    printf 'foo([1,2,3]).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- foo([A,B|T])'
    [ "$status" -eq 0 ]
    [[ "$output" == *"A = 1"* ]]
    [[ "$output" == *"B = 2"* ]]
    [[ "$output" == *"T = [3]"* ]]
}

@test "parse: nested lists" {
    printf 'foo([[a,b],[c]]).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- foo([[a,X],Y])'
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = b"* ]]
    [[ "$output" == *"Y = [c]"* ]]
}

# --- clauses ---

@test "parse: fact" {
    printf 'likes(mary, food).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- likes(mary, food)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "parse: rule single body" {
    printf 'a(x).\nb(X) :- a(X).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- b(X)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = x"* ]]
}

@test "parse: rule multiple body goals" {
    printf 'a(x).\nb(x).\nc(X) :- a(X), b(X).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- c(X)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = x"* ]]
}

# --- whitespace handling ---

@test "parse: extra spaces" {
    printf 'foo(  a  ,  b  ).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- foo( a , b )'
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "parse: tabs" {
    printf 'foo(\ta\t).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- foo(X)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = a"* ]]
}

@test "parse: no spaces around :-" {
    printf 'a(x).\nb(X):-a(X).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- b(X)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = x"* ]]
}

# --- query syntax ---

@test "parse: query single goal" {
    printf 'foo(a).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- foo(X)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = a"* ]]
}

@test "parse: query multiple goals" {
    printf 'foo(a).\nbar(a).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- foo(X), bar(X)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = a"* ]]
}

# --- edge cases ---

@test "parse: atom with underscore" {
    printf 'foo_bar(x).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- foo_bar(X)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = x"* ]]
}

@test "parse: atom with numbers" {
    printf 'foo123(x).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- foo123(X)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = x"* ]]
}

@test "parse: variable with underscore" {
    printf 'foo(a).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- foo(X_1)'
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
    printf 'foo(a).\n' > "$tmpfile"
    run bash -c "printf '?- @bad\n?- foo(X).\n' | $PROLOG -f \"$tmpfile\" 2>&1"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = a"* ]]
}

# --- multiline file loading ---

@test "parse file: rule body split across lines" {
    printf 'parent(tom, bob).\nancestor(X, Y) :-\n  parent(X, Y).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- ancestor(tom, bob)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "parse file: fact args split across lines" {
    printf 'foo(\n  a,\n  b,\n  c\n).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- foo(a, b, c)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "parse file: body goals each on own line" {
    printf 'a(x).\nb(x).\nc(X) :-\n  a(X),\n  b(X).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- c(X)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = x"* ]]
}

@test "parse file: percent comment on its own line" {
    printf '%% full-line comment\nfoo(a).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- foo(a)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "parse file: inline percent comment" {
    printf 'foo(a). %% this is ignored\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- foo(a)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "parse file: blank lines between clauses" {
    printf 'foo(a).\n\n\nbar(b).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- foo(a), bar(b)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "parse file: dot inside string does not end clause" {
    printf 'greeting("hello.world").\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- greeting(X)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"hello.world"* ]]
}

@test "parse file: multiple clauses loaded" {
    printf 'color(red).\ncolor(green).\ncolor(blue).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- color(green)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}
