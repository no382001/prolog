#!/usr/bin/env bats

setup() {
    PROLOG="./prolog"
}

# --- is/2 instantiation errors ---

@test "error: is/2 unbound rhs" {
    run bash -c "echo '?- X is Y' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"ERROR"* ]]
    [[ "$output" == *"instantiation_error"* ]]
}

@test "error: is/2 unbound in expression" {
    run bash -c "echo '?- X is Y + 1' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"ERROR"* ]]
    [[ "$output" == *"instantiation_error"* ]]
}

@test "error: is/2 unbound nested" {
    run bash -c "echo '?- X is 2 * (Y + 3)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"ERROR"* ]]
    [[ "$output" == *"instantiation_error"* ]]
}

@test "error: is/2 does not produce false" {
    run bash -c "echo '?- X is Y' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" != *"false"* ]]
}

# --- arithmetic comparison instantiation errors ---

@test "error: </2 unbound left" {
    run bash -c "echo '?- X < 3' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"instantiation_error"* ]]
}

@test "error: </2 unbound right" {
    run bash -c "echo '?- 3 < X' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"instantiation_error"* ]]
}

@test "error: >/2 unbound" {
    run bash -c "echo '?- X > 3' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"instantiation_error"* ]]
}

@test "error: =</2 unbound" {
    run bash -c "echo '?- X =< 3' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"instantiation_error"* ]]
}

@test "error: >=/2 unbound" {
    run bash -c "echo '?- X >= 3' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"instantiation_error"* ]]
}

@test "error: =:=/2 unbound" {
    run bash -c "echo '?- X =:= 3' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"instantiation_error"* ]]
}

@test "error: =\\=/2 unbound" {
    run bash -c "echo '?- X =\\= 3' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"instantiation_error"* ]]
}

# --- functor/3 instantiation errors ---

@test "error: functor/3 all unbound" {
    run bash -c "echo '?- functor(X, Y, Z)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"ERROR"* ]]
    [[ "$output" == *"instantiation_error"* ]]
}

@test "error: functor/3 unbound name" {
    run bash -c "echo '?- functor(X, Y, 2)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"instantiation_error"* ]]
}

@test "error: functor/3 unbound arity" {
    run bash -c "echo '?- functor(X, foo, Z)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"instantiation_error"* ]]
}

@test "error: functor/3 decompose still works" {
    run bash -c "echo '?- functor(foo(a,b), N, A)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"N = foo"* ]]
    [[ "$output" == *"A = 2"* ]]
}

@test "error: functor/3 compose still works" {
    run bash -c "echo '?- functor(T, foo, 2)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"T = foo"* ]]
}

# --- error propagation ---

@test "error: propagates through once/1" {
    run bash -c "echo '?- once(X is Y)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"instantiation_error"* ]]
}

@test "error: propagates through \\+" {
    run bash -c "echo '?- \+ (X is Y)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"instantiation_error"* ]]
}

@test "error: does not bleed into next query" {
    run bash -c "printf '?- X is Y.\n?- 1 < 2.\n' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"instantiation_error"* ]]
    [[ "$output" == *"true"* ]]
}

@test "error: bound variable does not error in is/2" {
    run bash -c "echo '?- Y = 5, X is Y + 1' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = 6"* ]]
    [[ "$output" != *"ERROR"* ]]
}

@test "error: bound variable does not error in </2" {
    run bash -c "echo '?- Y = 3, Y < 5' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"Y = 3"* ]]
    [[ "$output" != *"ERROR"* ]]
}
