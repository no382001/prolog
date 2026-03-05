#!/usr/bin/env bats

setup() {
    PROLOG="./prolog"
    tmpfile=$(mktemp /tmp/prolog_test_XXXXXX.pl)
}

teardown() {
    rm -f "$tmpfile"
}

@test "cut: prevents backtracking" {
    printf 'a(1).\na(2).\na(3).\nfirst(X) :- a(X), !.\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- first(X)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = 1"* ]]
}

@test "cut: in middle of clause" {
    printf 'a(1).\na(2).\nb(1).\nb(2).\ntest(X,Y) :- a(X), !, b(Y).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- test(1, Y)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"Y = 1"* ]]
}
