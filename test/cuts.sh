#!/usr/bin/env bats

setup() {
    PROLOG="./prolog"
}

@test "cut: prevents backtracking" {
    run bash -c "echo -e 'a(1).\na(2).\na(3).\nfirst(X) :- a(X), !.\n?- first(X)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = 1"* ]]
}

@test "cut: in middle of clause" {
    run bash -c "echo -e 'a(1).\na(2).\nb(1).\nb(2).\ntest(X,Y) :- a(X), !, b(Y).\n?- test(1, Y)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"Y = 1"* ]]
}