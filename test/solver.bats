#!/usr/bin/env bats

setup() {
    PROLOG="./prolog"
    CORE="test/core.pl"
}

# --- append tests ---

@test "append: two empty lists" {
    run $PROLOG -f $CORE -e "?- append([], [], X)"
    [ "$status" -eq 0 ]
    [ "$output" = "X = []" ]
}

@test "append: empty to non-empty" {
    run $PROLOG -f $CORE -e "?- append([], [1,2], X)"
    [ "$status" -eq 0 ]
    [ "$output" = "X = [1, 2]" ]
}

@test "append: non-empty to empty" {
    run $PROLOG -f $CORE -e "?- append([1,2], [], X)"
    [ "$status" -eq 0 ]
    [ "$output" = "X = [1, 2]" ]
}

@test "append: two non-empty lists" {
    run $PROLOG -f $CORE -e "?- append([1,2], [3,4], X)"
    [ "$status" -eq 0 ]
    [ "$output" = "X = [1, 2, 3, 4]" ]
}

@test "append: single elements" {
    run $PROLOG -f $CORE -e "?- append([a], [b], X)"
    [ "$status" -eq 0 ]
    [ "$output" = "X = [a, b]" ]
}

@test "append: verify known result" {
    run $PROLOG -f $CORE -e "?- append([1,2], [3], [1,2,3])"
    [ "$status" -eq 0 ]
    [ "$output" = "true" ]
}

@test "append: verify wrong result fails" {
    run $PROLOG -f $CORE -e "?- append([1,2], [3], [1,2,4])"
    [ "$status" -eq 0 ]
    [ "$output" = "false" ]
}

# --- member tests ---

@test "member: first element" {
    run $PROLOG -f $CORE -e "?- member(1, [1,2,3])"
    [ "$status" -eq 0 ]
    [ "$output" = "true" ]
}

@test "member: middle element" {
    run $PROLOG -f $CORE -e "?- member(2, [1,2,3])"
    [ "$status" -eq 0 ]
    [ "$output" = "true" ]
}

@test "member: last element" {
    run $PROLOG -f $CORE -e "?- member(3, [1,2,3])"
    [ "$status" -eq 0 ]
    [ "$output" = "true" ]
}

@test "member: not in list" {
    run $PROLOG -f $CORE -e "?- member(4, [1,2,3])"
    [ "$status" -eq 0 ]
    [ "$output" = "false" ]
}

@test "member: empty list" {
    run $PROLOG -f $CORE -e "?- member(1, [])"
    [ "$status" -eq 0 ]
    [ "$output" = "false" ]
}

@test "member: singleton list - found" {
    run $PROLOG -f $CORE -e "?- member(a, [a])"
    [ "$status" -eq 0 ]
    [ "$output" = "true" ]
}

@test "member: singleton list - not found" {
    run $PROLOG -f $CORE -e "?- member(b, [a])"
    [ "$status" -eq 0 ]
    [ "$output" = "false" ]
}

# --- combined/complex tests ---

@test "multiple goals: member and append" {
    run $PROLOG -f $CORE -e "?- append([1], [2], X), member(1, X)"
    [ "$status" -eq 0 ]
    [ "$output" = "X = [1, 2]" ]
}

@test "multiple goals: failing second goal" {
    run $PROLOG -f $CORE -e "?- append([1], [2], X), member(3, X)"
    [ "$status" -eq 0 ]
    [ "$output" = "false" ]
}

# --- facts only tests ---

@test "simple fact" {
    run bash -c "echo -e 'foo(a).\nfoo(b).\n?- foo(a)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "simple fact: query variable" {
    run bash -c "echo -e 'foo(a).\nfoo(b).\n?- foo(X)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = a"* ]]
}

@test "fact not found" {
    run bash -c "echo -e 'foo(a).\n?- foo(b)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

# --- nested structures ---

@test "nested functor" {
    run bash -c "echo -e 'f(g(a)).\n?- f(g(X))' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = a"* ]]
}

@test "nested functor: no match" {
    run bash -c "echo -e 'f(g(a)).\n?- f(h(X))' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

# --- piped input ---

@test "pipe: multiple lines" {
    run bash -c "printf 'parent(tom, bob).\nparent(bob, jim).\n?- parent(tom, X)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = bob"* ]]
}

@test "pipe: rule with body" {
    run bash -c "printf 'parent(tom, bob).\nparent(bob, jim).\ngrandparent(X,Z) :- parent(X,Y), parent(Y,Z).\n?- grandparent(tom, X)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = jim"* ]]
}