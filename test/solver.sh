#!/usr/bin/env bats

setup() {
    PROLOG="./prolog"
    tmpfile=$(mktemp /tmp/prolog_test_XXXXXX.pl)
}

teardown() {
    rm -f "$tmpfile"
}

# --- append tests ---

@test "append: two empty lists" {
    run $PROLOG -e "?- append([], [], X)"
    [ "$status" -eq 0 ]
    [ "$output" = "X = []" ]
}

@test "append: empty to non-empty" {
    run $PROLOG -e "?- append([], [1,2], X)"
    [ "$status" -eq 0 ]
    [ "$output" = "X = [1, 2]" ]
}

@test "append: non-empty to empty" {
    run $PROLOG -e "?- append([1,2], [], X)"
    [ "$status" -eq 0 ]
    [ "$output" = "X = [1, 2]" ]
}

@test "append: two non-empty lists" {
    run $PROLOG -e "?- append([1,2], [3,4], X)"
    [ "$status" -eq 0 ]
    [ "$output" = "X = [1, 2, 3, 4]" ]
}

@test "append: single elements" {
    run $PROLOG -e "?- append([a], [b], X)"
    [ "$status" -eq 0 ]
    [ "$output" = "X = [a, b]" ]
}

@test "append: verify known result" {
    run $PROLOG -e "?- append([1,2], [3], [1,2,3])"
    [ "$status" -eq 0 ]
    [ "$output" = "true" ]
}

@test "append: verify wrong result fails" {
    run $PROLOG -e "?- append([1,2], [3], [1,2,4])"
    [ "$status" -eq 0 ]
    [ "$output" = "false" ]
}

# --- member tests ---

@test "member: first element" {
    run $PROLOG -e "?- member(1, [1,2,3])"
    [ "$status" -eq 0 ]
    [ "$output" = "true" ]
}

@test "member: middle element" {
    run $PROLOG -e "?- member(2, [1,2,3])"
    [ "$status" -eq 0 ]
    [ "$output" = "true" ]
}

@test "member: last element" {
    run $PROLOG -e "?- member(3, [1,2,3])"
    [ "$status" -eq 0 ]
    [ "$output" = "true" ]
}

@test "member: not in list" {
    run $PROLOG -e "?- member(4, [1,2,3])"
    [ "$status" -eq 0 ]
    [ "$output" = "false" ]
}

@test "member: empty list" {
    run $PROLOG -e "?- member(1, [])"
    [ "$status" -eq 0 ]
    [ "$output" = "false" ]
}

@test "member: singleton list - found" {
    run $PROLOG -e "?- member(a, [a])"
    [ "$status" -eq 0 ]
    [ "$output" = "true" ]
}

@test "member: singleton list - not found" {
    run $PROLOG -e "?- member(b, [a])"
    [ "$status" -eq 0 ]
    [ "$output" = "false" ]
}

# --- combined/complex tests ---

@test "multiple goals: member and append" {
    run $PROLOG -e "?- append([1], [2], X), member(1, X)"
    [ "$status" -eq 0 ]
    [ "$output" = "X = [1, 2]" ]
}

@test "multiple goals: failing second goal" {
    run $PROLOG -e "?- append([1], [2], X), member(3, X)"
    [ "$status" -eq 0 ]
    [ "$output" = "false" ]
}

# --- facts only tests ---

@test "simple fact" {
    printf 'foo(a).\nfoo(b).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- foo(a)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "simple fact: query variable" {
    printf 'foo(a).\nfoo(b).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- foo(X)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = a"* ]]
}

@test "fact not found" {
    printf 'foo(a).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- foo(b)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

# --- nested structures ---

@test "nested functor" {
    printf 'f(g(a)).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- f(g(X))'
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = a"* ]]
}

@test "nested functor: no match" {
    printf 'f(g(a)).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- f(h(X))'
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

# --- piped input ---

@test "pipe: multiple lines" {
    printf 'parent(tom, bob).\nparent(bob, jim).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- parent(tom, X)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = bob"* ]]
}

@test "pipe: rule with body" {
    printf 'parent(tom, bob).\nparent(bob, jim).\ngrandparent(X,Z) :- parent(X,Y), parent(Y,Z).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- grandparent(tom, X)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = jim"* ]]
}
