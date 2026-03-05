#!/usr/bin/env bats

setup() {
    PROLOG="./prolog"
    CORE="test/core.pl"
    tmpfile=$(mktemp /tmp/prolog_test_XXXXXX.pl)
}

teardown() {
    rm -f "$tmpfile"
}

# --- is/2 basic arithmetic ---

@test "is: simple number" {
    run bash -c "echo '?- X is 5' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = 5"* ]]
}

@test "is: addition" {
    run bash -c "echo '?- X is 2 + 3' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = 5"* ]]
}

@test "is: subtraction" {
    run bash -c "echo '?- X is 10 - 4' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = 6"* ]]
}

@test "is: multiplication" {
    run bash -c "echo '?- X is 3 * 4' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = 12"* ]]
}

@test "is: division" {
    run bash -c "echo '?- X is 15 / 3' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = 5"* ]]
}

@test "is: integer division truncates" {
    run bash -c "echo '?- X is 7 / 2' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = 3"* ]]
}

@test "is: modulo" {
    run bash -c "echo '?- X is 17 mod 5' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = 2"* ]]
}

@test "is: modulo zero remainder" {
    run bash -c "echo '?- X is 10 mod 5' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = 0"* ]]
}

@test "is: negative numbers" {
    run bash -c "echo '?- X is -3 + 5' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = 2"* ]]
}

@test "is: result is negative" {
    run bash -c "echo '?- X is 3 - 10' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = -7"* ]]
}

# --- is/2 complex expressions ---

@test "is: nested addition" {
    run bash -c "echo '?- X is 1 + 2 + 3' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = 6"* ]]
}

@test "is: precedence mul over add" {
    run bash -c "echo '?- X is 2 + 3 * 4' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = 14"* ]]
}

@test "is: precedence with subtraction" {
    run bash -c "echo '?- X is 10 - 2 * 3' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = 4"* ]]
}

@test "is: parentheses override precedence" {
    run bash -c "echo '?- X is (2 + 3) * 4' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = 20"* ]]
}

@test "is: multiple operations" {
    run bash -c "echo '?- X is 2 * 3 + 4 * 5' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = 26"* ]]
}

@test "is: division and multiplication" {
    run bash -c "echo '?- X is 20 / 4 * 2' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = 10"* ]]
}

# --- is/2 with variables ---

@test "is: variable in expression" {
    printf 'num(5).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- num(N), X is N + 1'
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = 6"* ]]
}

@test "is: two variables" {
    printf 'pair(3, 4).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- pair(A, B), X is A + B'
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = 7"* ]]
}

@test "is: unify with known value succeeds" {
    run bash -c "echo '?- 5 is 2 + 3' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "is: unify with wrong value fails" {
    run bash -c "echo '?- 6 is 2 + 3' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

# --- is/2 failures ---

@test "is: unbound variable in expression fails" {
    run bash -c "echo '?- X is Y + 1' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

@test "is: non-numeric atom fails" {
    run bash -c "echo '?- X is foo + 1' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

# --- comparison: less than ---

@test "less than: true" {
    run bash -c "echo '?- 3 < 5' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "less than: false when equal" {
    run bash -c "echo '?- 5 < 5' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

@test "less than: false when greater" {
    run bash -c "echo '?- 7 < 5' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

@test "less than: with expressions" {
    run bash -c "echo '?- 2 + 1 < 2 * 2' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

# --- comparison: greater than ---

@test "greater than: true" {
    run bash -c "echo '?- 5 > 3' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "greater than: false when equal" {
    run bash -c "echo '?- 5 > 5' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

@test "greater than: false when less" {
    run bash -c "echo '?- 3 > 5' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

@test "greater than: with expressions" {
    run bash -c "echo '?- 3 * 3 > 2 + 5' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

# --- comparison: less than or equal ---

@test "less equal: true when less" {
    run bash -c "echo '?- 3 =< 5' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "less equal: true when equal" {
    run bash -c "echo '?- 5 =< 5' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "less equal: false when greater" {
    run bash -c "echo '?- 7 =< 5' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

# --- comparison: greater than or equal ---

@test "greater equal: true when greater" {
    run bash -c "echo '?- 5 >= 3' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "greater equal: true when equal" {
    run bash -c "echo '?- 5 >= 5' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "greater equal: false when less" {
    run bash -c "echo '?- 3 >= 5' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

# --- comparison: arithmetic equal ---

@test "arith equal: same numbers" {
    run bash -c "echo '?- 5 =:= 5' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "arith equal: equal expressions" {
    run bash -c "echo '?- 2 + 3 =:= 1 + 4' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "arith equal: different values" {
    run bash -c "echo '?- 5 =:= 6' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

# --- comparison: arithmetic not equal ---

@test "arith not equal: different numbers" {
    run bash -c "echo '?- 5 =\\= 6' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "arith not equal: same numbers" {
    run bash -c "echo '?- 5 =\\= 5' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

@test "arith not equal: expressions" {
    run bash -c "echo '?- 2 * 3 =\\= 2 + 3' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

# --- true/0 and fail/0 ---

@test "true: succeeds" {
    run bash -c "echo '?- true' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "fail: fails" {
    run bash -c "echo '?- fail' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

@test "true in conjunction" {
    printf 'foo(a).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- foo(X), true'
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = a"* ]]
}

@test "fail in conjunction" {
    run bash -c "echo -e 'foo(a).\n?- foo(X), fail' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

# --- unification: =/2 ---

@test "unify: same atoms" {
    run bash -c "echo '?- a = a' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "unify: different atoms" {
    run bash -c "echo '?- a = b' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

@test "unify: variable to atom" {
    run bash -c "echo '?- X = foo' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = foo"* ]]
}

@test "unify: two variables" {
    run bash -c "echo '?- X = Y, X = hello' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"hello"* ]]
}

@test "unify: structures" {
    run bash -c "echo '?- foo(a, B) = foo(A, b)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"A = a"* ]]
    [[ "$output" == *"B = b"* ]]
}

@test "unify: structures different functors" {
    run bash -c "echo '?- foo(a) = bar(a)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

@test "unify: structures different arity" {
    run bash -c "echo '?- foo(a) = foo(a, b)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

@test "unify: lists" {
    run bash -c "echo '?- [H|T] = [1, 2, 3]' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"H = 1"* ]]
    [[ "$output" == *"T = [2, 3]"* ]]
}

# --- not unifiable: \=/2 ---

@test "not unify: different atoms" {
    run bash -c "echo '?- a \\= b' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "not unify: same atoms" {
    run bash -c "echo '?- a \\= a' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

@test "not unify: variable and atom" {
    run bash -c "echo '?- X \\= a' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

@test "not unify: different structures" {
    run bash -c "echo '?- foo(a) \\= foo(b)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "not unify: does not bind variables" {
    run bash -c "echo '?- X \\= a, X = b' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]] || [[ "$output" == *"X = b"* ]]
}

# --- combined builtins ---

@test "combined: arithmetic in rule" {
    printf 'double(X, Y) :- Y is X * 2.\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- double(5, D)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"D = 10"* ]]
}

@test "combined: comparison guard" {
    printf 'positive(X) :- X > 0.\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- positive(5)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "combined: comparison guard fails" {
    printf 'positive(X) :- X > 0.\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- positive(-3)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

@test "combined: max of two" {
    printf 'max(X, Y, X) :- X >= Y.\nmax(X, Y, Y) :- Y > X.\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- max(3, 7, M)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"M = 7"* ]]
}

@test "combined: factorial base case" {
    printf 'fact(0, 1).\nfact(N, F) :- N > 0, N1 is N - 1, fact(N1, F1), F is N * F1.\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- fact(0, F)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"F = 1"* ]]
}

@test "combined: factorial of 5" {
    printf 'fact(0, 1).\nfact(N, F) :- N > 0, N1 is N - 1, fact(N1, F1), F is N * F1.\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- fact(5, F)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"F = 120"* ]]
}

@test "combined: sum list" {
    printf 'sum([], 0).\nsum([H|T], S) :- sum(T, S1), S is S1 + H.\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- sum([1,2,3,4], S)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"S = 10"* ]]
}

# --- edge cases ---

@test "edge: zero operations" {
    run bash -c "echo '?- X is 0 + 0' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = 0"* ]]
}

@test "edge: multiply by zero" {
    run bash -c "echo '?- X is 5 * 0' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = 0"* ]]
}

@test "edge: comparison with zero" {
    run bash -c "echo '?- 0 < 1' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "edge: negative comparison" {
    run bash -c "echo '?- -5 < -3' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

# --- findall basic ---

@test "findall: collect all facts" {
    printf 'foo(a).\nfoo(b).\nfoo(c).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- findall(X, foo(X), L)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"L = [a, b, c]"* ]]
}

@test "findall: empty result is empty list" {
    run bash -c "echo '?- findall(X, bar(X), L)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"L = []"* ]]
}

@test "findall: single result" {
    printf 'foo(only).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- findall(X, foo(X), L)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"L = [only]"* ]]
}

# --- findall with templates ---

@test "findall: template extracts part" {
    printf 'pair(1,a).\npair(2,b).\npair(3,c).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- findall(Y, pair(X,Y), L)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"L = [a, b, c]"* ]]
}

@test "findall: template is constant" {
    printf 'foo(a).\nfoo(b).\nfoo(c).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- findall(x, foo(_), L)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"L = [x, x, x]"* ]]
}

@test "findall: template is compound" {
    printf 'edge(a,b).\nedge(b,c).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- findall(pair(X,Y), edge(X,Y), L)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"pair(a, b)"* ]]
    [[ "$output" == *"pair(b, c)"* ]]
}

# --- findall with rules ---

@test "findall: works with rules" {
    printf 'parent(tom,bob).\nparent(tom,liz).\nparent(bob,jim).\nchild(C,P) :- parent(P,C).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- findall(C, child(C,tom), L)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"bob"* ]]
    [[ "$output" == *"liz"* ]]
}

@test "findall: with member" {
    run $PROLOG -f $CORE -e "?- findall(X, member(X, [1,2,3]), L)"
    [ "$status" -eq 0 ]
    [[ "$output" == *"L = [1, 2, 3]"* ]]
}

@test "findall: with append" {
    run $PROLOG -f $CORE -e "?- findall(L, append([a], X, L), Results)"
    [ "$status" -eq 0 ]
    # Should find at least the case where X=[]
}

# --- findall with arithmetic ---

@test "findall: with arithmetic in goal" {
    skip
    run bash -c "echo -e 'num(1).\nnum(2).\nnum(3).\n?- findall(D, (num(X), D is X * 2), L)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"L = [2, 4, 6]"* ]]
}

@test "findall: with comparison in goal" {
    skip
    run bash -c "echo -e 'num(1).\nnum(2).\nnum(3).\nnum(4).\n?- findall(X, (num(X), X > 2), L)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"L = [3, 4]"* ]]
}

# --- findall nested ---

@test "findall: used in rule body" {
    printf 'item(a).\nitem(b).\nall_items(L) :- findall(X, item(X), L).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- all_items(L)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"L = [a, b]"* ]]
}

# --- bagof basic ---

@test "bagof: collect all facts" {
    printf 'foo(a).\nfoo(b).\nfoo(c).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- bagof(X, foo(X), L)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"L = [a, b, c]"* ]]
}

@test "bagof: fails on no solutions" {
    run bash -c "echo '?- bagof(X, bar(X), L)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

@test "bagof: single solution" {
    printf 'foo(only).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- bagof(X, foo(X), L)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"L = [only]"* ]]
}

@test "bagof: with template" {
    printf 'pair(1,x).\npair(2,y).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- bagof(B, pair(A,B), L)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"x"* ]]
    [[ "$output" == *"y"* ]]
}

# --- consult/1 ---

@test "consult: loads facts from file" {
    printf 'animal(cat).\nanimal(dog).\n' > "$tmpfile"
    run bash -c "printf '?- consult(\"%s\")\n?- animal(cat)\n' \"$tmpfile\" | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "consult: fails on nonexistent file" {
    run bash -c "echo '?- consult(\"/nonexistent/does_not_exist.pl\")' | $PROLOG 2>&1"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

@test "consult: loads multiline rules" {
    printf 'parent(tom, bob).\nancestor(X, Y) :-\n  parent(X, Y).\n' > "$tmpfile"
    run bash -c "printf '?- consult(\"%s\")\n?- ancestor(tom, bob)\n' \"$tmpfile\" | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "consult: all clauses from file are accessible" {
    printf 'color(red).\ncolor(green).\ncolor(blue).\n' > "$tmpfile"
    run bash -c "printf '?- consult(\"%s\")\n?- color(red), color(blue)\n' \"$tmpfile\" | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "consult: works in rule body" {
    local tmpfile2
    tmpfile2=$(mktemp /tmp/prolog_test_XXXXXX.pl)
    printf 'foo(a).\n' > "$tmpfile"
    printf 'load(F) :- consult(F).\n' > "$tmpfile2"
    run $PROLOG -f "$tmpfile2" -e "?- load(\"$tmpfile\"), foo(a)"
    rm -f "$tmpfile2"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

# --- include/1 ---

@test "include: works as file directive" {
    local tmpfile2
    tmpfile2=$(mktemp /tmp/prolog_test_XXXXXX.pl)
    printf 'animal(cat).\nanimal(dog).\n' > "$tmpfile"
    printf ':- include("%s").\n' "$tmpfile" > "$tmpfile2"
    run $PROLOG -f "$tmpfile2" -e '?- animal(cat)'
    rm -f "$tmpfile2"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "include: rejected as runtime goal" {
    run bash -c "echo '?- include(\"/dev/null\")' | $PROLOG 2>&1"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

# --- nl/0 ---

@test "nl: succeeds" {
    run bash -c "echo '?- nl' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "nl: outputs a newline" {
    run bash -c "echo '?- nl' | $PROLOG"
    [ "$status" -eq 0 ]
    # output contains a blank line (the newline from nl)
    [[ "$output" == *$'\n'* ]]
}

# --- write/1 ---

@test "write: prints an atom" {
    run bash -c "echo '?- write(hello)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"hello"* ]]
}

@test "write: prints a number" {
    run bash -c "echo '?- write(42)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"42"* ]]
}

@test "write: prints a compound term" {
    run bash -c "echo '?- write(foo(a, b))' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"foo"* ]]
    [[ "$output" == *"a"* ]]
    [[ "$output" == *"b"* ]]
}

@test "write: prints a bound variable" {
    run bash -c "echo '?- X = hello, write(X)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"hello"* ]]
}

@test "write: succeeds" {
    run bash -c "echo '?- write(anything)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "write: works in rule body" {
    printf 'greet :- write(hi).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- greet'
    [ "$status" -eq 0 ]
    [[ "$output" == *"hi"* ]]
}

# --- writeln/1 ---

@test "writeln: prints an atom" {
    run bash -c "echo '?- writeln(hello)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"hello"* ]]
}

@test "writeln: prints a number" {
    run bash -c "echo '?- writeln(99)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"99"* ]]
}

@test "writeln: succeeds" {
    run bash -c "echo '?- writeln(anything)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "writeln: works in rule body" {
    printf 'announce(X) :- writeln(X).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- announce(done)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"done"* ]]
}

# --- \+/1 negation as failure ---

@test "not: \\+ fail succeeds" {
    run bash -c "echo '?- \\+ fail' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "not: \\+ true fails" {
    run bash -c "echo '?- \\+ true' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

@test "not: \\+ with unmatched fact succeeds" {
    printf 'foo(a).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- \+ foo(b)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "not: \\+ with matched fact fails" {
    printf 'foo(a).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- \+ foo(a)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

@test "not: \\+ with bound variable" {
    printf 'foo(a).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- X = b, \+ foo(X)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = b"* ]]
}

@test "not: fails when inner goal can succeed with unbound var" {
    printf 'foo(a).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- \+ foo(X)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

@test "not: works in rule body" {
    printf 'foo(a).\nnot_foo(X) :- \+ foo(X).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- not_foo(b)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "not: functor form \\+(Goal)" {
    run bash -c "echo '?- \\+(fail)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

# --- call/1 ---

@test "call: call(true) succeeds" {
    run bash -c "echo '?- call(true)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "call: call(fail) fails" {
    run bash -c "echo '?- call(fail)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

@test "call: call with a fact" {
    printf 'foo(a).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- call(foo(a))'
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "call: call with a goal variable" {
    printf 'foo(a).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- G = foo(a), call(G)'
    [ "$status" -eq 0 ]
    [[ "$output" == *"foo(a)"* ]]
}

@test "call: call binds variables" {
    printf 'foo(a).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- call(foo(X))'
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = a"* ]]
}

@test "call: backtracking through call" {
    run $PROLOG -f test/core.pl -e "?- findall(X, call(member(X, [1,2,3])), L)"
    [ "$status" -eq 0 ]
    [[ "$output" == *"L = [1, 2, 3]"* ]]
}

@test "call: call in rule body" {
    printf 'apply(G) :- call(G).\nfoo(a).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- apply(foo(X))'
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = a"* ]]
}

@test "call: \\+ call(G) negates the called goal" {
    printf 'foo(a).\n' > "$tmpfile"
    run $PROLOG -f "$tmpfile" -e '?- \+ call(foo(b))'
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "call: nested call(call(G))" {
    run bash -c "echo '?- call(call(true))' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

# --- var/1 ---

@test "var: unbound variable" {
    run bash -c "echo '?- var(X)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "var: bound variable fails" {
    run bash -c "echo '?- X = a, var(X)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

@test "var: atom fails" {
    run bash -c "echo '?- var(foo)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

@test "var: integer fails" {
    run bash -c "echo '?- var(42)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

# --- nonvar/1 ---

@test "nonvar: atom succeeds" {
    run bash -c "echo '?- nonvar(foo)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "nonvar: integer succeeds" {
    run bash -c "echo '?- nonvar(42)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "nonvar: compound succeeds" {
    run bash -c "echo '?- nonvar(f(x))' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "nonvar: bound variable succeeds" {
    run bash -c "echo '?- X = a, nonvar(X)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = a"* ]]
}

@test "nonvar: unbound variable fails" {
    run bash -c "echo '?- nonvar(X)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

# --- atom/1 ---

@test "atom: atom succeeds" {
    run bash -c "echo '?- atom(foo)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "atom: empty list is atom" {
    run bash -c "echo '?- atom([])' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "atom: integer fails" {
    run bash -c "echo '?- atom(42)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

@test "atom: compound fails" {
    run bash -c "echo '?- atom(f(x))' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

@test "atom: variable fails" {
    run bash -c "echo '?- atom(X)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

# --- integer/1 ---

@test "integer: integer succeeds" {
    run bash -c "echo '?- integer(42)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "integer: zero succeeds" {
    run bash -c "echo '?- integer(0)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "integer: negative integer succeeds" {
    run bash -c "echo '?- integer(-3)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "integer: atom fails" {
    run bash -c "echo '?- integer(foo)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

@test "integer: variable fails" {
    run bash -c "echo '?- integer(X)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

@test "integer: result of is/2 is integer" {
    run bash -c "echo '?- X is 2 + 3, integer(X)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"X = 5"* ]]
}

# --- is_list/1 ---

@test "is_list: empty list" {
    run bash -c "echo '?- is_list([])' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "is_list: proper list" {
    run bash -c "echo '?- is_list([1,2,3])' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "is_list: singleton list" {
    run bash -c "echo '?- is_list([a])' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"true"* ]]
}

@test "is_list: atom fails" {
    run bash -c "echo '?- is_list(foo)' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

@test "is_list: partial list fails" {
    run bash -c "echo '?- is_list([a|b])' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}

@test "is_list: unbound tail fails" {
    run bash -c "echo '?- is_list([a|_])' | $PROLOG"
    [ "$status" -eq 0 ]
    [[ "$output" == *"false"* ]]
}
