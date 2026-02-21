#!/usr/bin/env bats

setup() {
    PROLOG="./prolog"
    CORE="test/core.pl"
}

# --- between/3 ---

@test "between: generates values in range" {
    run $PROLOG -f $CORE -e "?- findall(X, between(1,5,X), L)"
    [ "$status" -eq 0 ]
    [[ "$output" == *"L = [1, 2, 3, 4, 5]"* ]]
}

@test "between: single value range" {
    run $PROLOG -f $CORE -e "?- findall(X, between(3,3,X), L)"
    [ "$status" -eq 0 ]
    [[ "$output" == *"L = [3]"* ]]
}

@test "between: empty range fails" {
    run $PROLOG -f $CORE -e "?- between(5,3,_)"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

@test "between: check membership" {
    run $PROLOG -f $CORE -e "?- between(1,10,5)"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "between: check out of range fails" {
    run $PROLOG -f $CORE -e "?- between(1,10,11)"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

@test "between: negative range" {
    run $PROLOG -f $CORE -e "?- findall(X, between(-2,2,X), L)"
    [ "$status" -eq 0 ]
    [[ "$output" == *"-2"* ]]
    [[ "$output" == *"2"* ]]
}

# --- once/1 ---

@test "once: succeeds on first solution only" {
    run $PROLOG -f $CORE -e "?- once(member(X, [a,b,c]))"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = a"* ]]
    [[ "$output" != *"b"* ]]
}

@test "once: succeeds when goal succeeds" {
    run $PROLOG -f $CORE -e "?- once(true)"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "once: fails when goal fails" {
    run $PROLOG -f $CORE -e "?- once(fail)"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

@test "once: cuts alternatives" {
    run $PROLOG -f $CORE -e "?- findall(X, once(member(X,[1,2,3])), L)"
    [ "$status" -eq 0 ]
    [[ "$output" == *"L = [1]"* ]]
}

# --- forall/2 ---

@test "forall: all elements satisfy condition" {
    run $PROLOG -f $CORE -e "?- forall(member(X,[2,4,6]), 0 =:= X mod 2)"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "forall: fails when one element does not satisfy" {
    run $PROLOG -f $CORE -e "?- forall(member(X,[2,3,6]), 0 =:= X mod 2)"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

@test "forall: vacuously true on empty domain" {
    run $PROLOG -f $CORE -e "?- forall(member(_,[]), fail)"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "forall: works with between" {
    run $PROLOG -f $CORE -e "?- forall(between(1,5,X), X > 0)"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}
