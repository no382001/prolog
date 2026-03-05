#!/usr/bin/env bats

setup() {
    PROLOG="./prolog"
    CORE="test/core.pl"
    tmpfile=$(mktemp /tmp/prolog_test_XXXXXX.pl)
}

teardown() {
    rm -f "$tmpfile"
}


@test "strings: simple string" {
    printf 'str("hello").\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- str("hello")'
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "strings: empty string" {
    printf 'str("").\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- str("")'
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "strings: string with spaces" {
    printf 'str("hello world").\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- str("hello world")'
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "strings: string with numbers" {
    printf 'str("test123").\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- str("test123")'
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "strings: string with special chars" {
    printf 'str("hello!@#").\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- str("hello!@#")'
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}


# --- String escape sequences ---

@test "strings: escaped newline" {
    printf 'str("hello\\nworld").\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- str("hello\nworld")'
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "strings: escaped tab" {
    printf 'str("hello\\tworld").\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- str("hello\tworld")'
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "strings: escaped backslash" {
    printf 'str("hello\\\\world").\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- str("hello\\world")'
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "strings: escaped quote" {
    run $PROLOG -e '?- X = "\""'
    [ "$status" -eq 0 ]
    [[ "$output" == *'X = "\""'* ]]
}


# --- Escape sequence encoding/decoding ---

@test "strings: escape: newline round-trips" {
    run $PROLOG -e '?- X = "\n"'
    [ "$status" -eq 0 ]
    [[ "$output" == *'X = "\n"'* ]]
}

@test "strings: escape: tab round-trips" {
    run $PROLOG -e '?- X = "\t"'
    [ "$status" -eq 0 ]
    [[ "$output" == *'X = "\t"'* ]]
}

@test "strings: escape: carriage return round-trips" {
    run $PROLOG -e '?- X = "\r"'
    [ "$status" -eq 0 ]
    [[ "$output" == *'X = "\r"'* ]]
}

@test "strings: escape: backslash round-trips" {
    run $PROLOG -e '?- X = "\\"'
    [ "$status" -eq 0 ]
    [[ "$output" == *'X = "\\"'* ]]
}

@test "strings: escape: write prints raw bytes" {
    run $PROLOG -e '?- write("hello\nworld")'
    [ "$status" -eq 0 ]
    [[ "$output" == *$'hello\nworld'* ]]
}

@test "strings: escape: writeq prints escape sequences" {
    run $PROLOG -e '?- writeq("\n\t\r")'
    [ "$status" -eq 0 ]
    [[ "$output" == *'"\n\t\r"'* ]]
}

@test "strings: escape: newline is one character" {
    run $PROLOG -f $CORE -e '?- length("\n", N)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"N = 1"* ]]
}

@test "strings: escape: backslash is one character" {
    run $PROLOG -f $CORE -e '?- length("\\", N)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"N = 1"* ]]
}

@test "strings: escape: mixed escapes" {
    run $PROLOG -e '?- X = "a\nb"'
    [ "$status" -eq 0 ]
    [[ "$output" == *'X = "a\nb"'* ]]
}

@test "strings: escape: tail of newline string is empty" {
    run $PROLOG -e '?- [_|T] = "\n"'
    [ "$status" -eq 0 ]
    [[ "$output" == *'T = ""'* ]]
}


# --- String unification with variables ---

@test "strings: unify string with variable" {
    printf 'str("hello").\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- str(X)'
    [ "$status" -eq 0 ]
    [[ "$output" == *'X = "hello"'* ]]
}

@test "strings: unify variable with string query" {
    printf 'str(X) :- X = "test".\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- str(Y)'
    [ "$status" -eq 0 ]
    [[ "$output" == *'Y = "test"'* ]]
}

@test "strings: multiple string variables" {
    printf 'pair("foo", "bar").\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- pair(X, Y)'
    [ "$status" -eq 0 ]
    [[ "$output" == *'X = "foo"'* ]]
    [[ "$output" == *'Y = "bar"'* ]]
}


# --- String comparison ---

@test "strings: identical strings unify" {
    printf 'test :- "hello" = "hello".\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- test'
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
    printf 'person("Alice", 30).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- person("Alice", 30)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "strings: query string in functor" {
    printf 'person("Alice", 30).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- person(X, 30)'
    [ "$status" -eq 0 ]
    [[ "$output" == *'X = "Alice"'* ]]
}

@test "strings: multiple strings in functor" {
    printf 'greeting("hello", "world").\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- greeting(X, Y)'
    [ "$status" -eq 0 ]
    [[ "$output" == *'X = "hello"'* ]]
    [[ "$output" == *'Y = "world"'* ]]
}

@test "strings: nested functors with strings" {
    printf 'data(user("Bob")).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- data(user(X))'
    [ "$status" -eq 0 ]
    [[ "$output" == *'X = "Bob"'* ]]
}


# --- Strings in lists ---

@test "strings: list of strings" {
    printf 'list(["a", "b", "c"]).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- list(["a", "b", "c"])'
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "strings: query list with strings" {
    printf 'list(["foo", "bar"]).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- list(X)'
    [ "$status" -eq 0 ]
    [[ "$output" == *'X = ["foo", "bar"]'* ]]
}

@test "strings: unify list element" {
    printf 'list(["first", "second"]).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- list([X, Y])'
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
    printf 'data("text", 42, atom).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- data(X, Y, Z)'
    [ "$status" -eq 0 ]
    [[ "$output" == *'X = "text"'* ]]
    [[ "$output" == *"Y = 42"* ]]
    [[ "$output" == *"Z = atom"* ]]
}


# --- Edge cases ---

@test "strings: string with single character" {
    printf 'str("x").\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- str("x")'
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "strings: string with parentheses" {
    printf 'str("(test)").\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- str("(test)")'
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "strings: string with brackets" {
    printf 'str("[test]").\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- str("[test]")'
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "strings: string with comma" {
    printf 'str("a,b,c").\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- str("a,b,c")'
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}


# --- Multiple solutions with strings ---

@test "strings: multiple clauses with strings" {
    printf 'color("red").\ncolor("green").\ncolor("blue").\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- color(X)'
    [ "$status" -eq 0 ]
    [[ "$output" == *'X = "red"'* ]]
}

@test "strings: backtracking with strings" {
    printf 'msg("hello").\nmsg("goodbye").\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- msg(X)'
    [ "$status" -eq 0 ]
    [[ "$output" == *'X = "hello"'* ]]
}


# --- String-as-list unification ---

@test "strings: head|tail unification" {
    run bash -c "printf '?- [L|Ls] = \"abc\".' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"L = a"* ]]
    [[ "$output" == *'Ls = "bc"'* ]]
}

@test "strings: unify string with full char list" {
    run bash -c "printf '?- \"abc\" = [a,b,c].' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "strings: head of string is first char" {
    run bash -c "printf '?- [H|_] = \"hello\".' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"H = h"* ]]
}

@test "strings: tail of string is rest as string" {
    run bash -c "printf '?- [_|T] = \"hello\".' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *'T = "ello"'* ]]
}

@test "strings: empty string unifies with empty list" {
    run bash -c "printf '?- \"\" = [].' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "strings: non-empty string does not unify with empty list" {
    run bash -c "printf '?- \"a\" = [].' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

@test "strings: single char string head|tail" {
    run bash -c "printf '?- [L|Ls] = \"x\".' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"L = x"* ]]
    [[ "$output" == *'Ls = ""'* ]]
}

@test "strings: wrong char list does not unify" {
    run bash -c "printf '?- \"abc\" = [a,b,d].' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

@test "strings: multiple heads from string" {
    run $PROLOG -e '?- [A,B,C|Rest] = "prolog"'
    [ "$status" -eq 0 ]
    [[ "$output" == *"A = p"* ]]
    [[ "$output" == *"B = r"* ]]
    [[ "$output" == *"C = o"* ]]
    [[ "$output" == *'Rest = "log"'* ]]
}

@test "strings: char list = string" {
    run $PROLOG -e '?- [h,e,l,l,o] = "hello"'
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "strings: member on string" {
    run $PROLOG -f $CORE -e '?- member(X, "abc")'
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = a"* ]]
}

@test "strings: findall chars from string" {
    run $PROLOG -f $CORE -e '?- findall(X, member(X, "abc"), Cs)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"Cs = [a, b, c]"* ]]
}

@test "strings: length of string" {
    run $PROLOG -f $CORE -e '?- length("hello", N)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"N = 5"* ]]
}

@test "strings: length of empty string" {
    run $PROLOG -f $CORE -e '?- length("", N)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"N = 0"* ]]
}

@test "strings: reverse of string gives char list" {
    run $PROLOG -f $CORE -e '?- reverse("abc", X)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = [c, b, a]"* ]]
}

@test "strings: last char of string" {
    run $PROLOG -f $CORE -e '?- last(X, "hello")'
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = o"* ]]
}

@test "strings: member check on string" {
    run $PROLOG -f $CORE -e '?- member(h, "hello")'
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "strings: non-member check on string" {
    run $PROLOG -f $CORE -e '?- member(z, "hello")'
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}
