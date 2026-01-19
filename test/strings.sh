#!/usr/bin/env bats

setup() {
    PROLOG="./prolog"
}


@test "strings: simple string" {
    run bash -c "printf 'str(\"hello\").\n?- str(\"hello\")' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "strings: empty string" {
    run bash -c "printf 'str(\"\").\n?- str(\"\")' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "strings: string with spaces" {
    run bash -c "printf 'str(\"hello world\").\n?- str(\"hello world\")' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "strings: string with numbers" {
    run bash -c "printf 'str(\"test123\").\n?- str(\"test123\")' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "strings: string with special chars" {
    run bash -c "printf 'str(\"hello!@#\").\n?- str(\"hello!@#\")' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}


# --- String escape sequences ---

@test "strings: escaped newline" {
    run bash -c "printf 'str(\"hello\\\\nworld\").\n?- str(\"hello\\\\nworld\")' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "strings: escaped tab" {
    run bash -c "printf 'str(\"hello\\\\tworld\").\n?- str(\"hello\\\\tworld\")' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "strings: escaped backslash" {
    run bash -c "printf 'str(\"hello\\\\\\\\world\").\n?- str(\"hello\\\\\\\\world\")' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "strings: escaped quote" {
    skip
    run bash -c "printf 'str(\"hello\\\\\"world\").\n?- str(\"hello\\\\\"world\")' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}


# --- String unification with variables ---

@test "strings: unify string with variable" {
    run bash -c "printf 'str(\"hello\").\n?- str(X)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *'X = "hello"'* ]]
}

@test "strings: unify variable with string query" {
    run bash -c "printf 'str(X) :- X = \"test\".\n?- str(Y)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *'Y = "test"'* ]]
}

@test "strings: multiple string variables" {
    run bash -c "printf 'pair(\"foo\", \"bar\").\n?- pair(X, Y)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *'X = "foo"'* ]]
    [[ "$output" == *'Y = "bar"'* ]]
}


# --- String comparison ---

@test "strings: identical strings unify" {
    run bash -c "printf 'test :- \"hello\" = \"hello\".\n?- test' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "strings: different strings don't unify" {
    run bash -c "printf 'test :- \"hello\" = \"world\".\n?- test' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

@test "strings: case sensitive comparison" {
    run bash -c "printf 'test :- \"Hello\" = \"hello\".\n?- test' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}


# --- Strings in complex terms ---

@test "strings: string as functor argument" {
    run bash -c "printf 'person(\"Alice\", 30).\n?- person(\"Alice\", 30)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "strings: query string in functor" {
    run bash -c "printf 'person(\"Alice\", 30).\n?- person(X, 30)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *'X = "Alice"'* ]]
}

@test "strings: multiple strings in functor" {
    run bash -c "printf 'greeting(\"hello\", \"world\").\n?- greeting(X, Y)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *'X = "hello"'* ]]
    [[ "$output" == *'Y = "world"'* ]]
}

@test "strings: nested functors with strings" {
    run bash -c "printf 'data(user(\"Bob\")).\n?- data(user(X))' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *'X = "Bob"'* ]]
}


# --- Strings in lists ---

@test "strings: list of strings" {
    run bash -c "printf 'list([\"a\", \"b\", \"c\"]).\n?- list([\"a\", \"b\", \"c\"])' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "strings: query list with strings" {
    run bash -c "printf 'list([\"foo\", \"bar\"]).\n?- list(X)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *'X = ["foo", "bar"]'* ]]
}

@test "strings: unify list element" {
    run bash -c "printf 'list([\"first\", \"second\"]).\n?- list([X, Y])' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *'X = "first"'* ]]
    [[ "$output" == *'Y = "second"'* ]]
}


# --- Mixed types ---

@test "strings: string doesn't unify with atom" {
    run bash -c "printf 'test :- \"atom\" = atom.\n?- test' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

@test "strings: string doesn't unify with number" {
    run bash -c "printf 'test :- \"42\" = 42.\n?- test' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

@test "strings: mixed types in functor" {
    run bash -c "printf 'data(\"text\", 42, atom).\n?- data(X, Y, Z)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *'X = "text"'* ]]
    [[ "$output" == *"Y = 42"* ]]
    [[ "$output" == *"Z = atom"* ]]
}


# --- Edge cases ---

@test "strings: string with single character" {
    run bash -c "printf 'str(\"x\").\n?- str(\"x\")' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "strings: string with parentheses" {
    run bash -c "printf 'str(\"(test)\").\n?- str(\"(test)\")' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "strings: string with brackets" {
    run bash -c "printf 'str(\"[test]\").\n?- str(\"[test]\")' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "strings: string with comma" {
    run bash -c "printf 'str(\"a,b,c\").\n?- str(\"a,b,c\")' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}


# --- Multiple solutions with strings ---

@test "strings: multiple clauses with strings" {
    run bash -c "printf 'color(\"red\").\ncolor(\"green\").\ncolor(\"blue\").\n?- color(X)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *'X = "red"'* ]]
}

@test "strings: backtracking with strings" {
    run bash -c "printf 'msg(\"hello\").\nmsg(\"goodbye\").\n?- msg(X)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *'X = "hello"'* ]]
}
