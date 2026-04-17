#!/usr/bin/env bats

TRILOG="./trilog"

# --- exit code contract ---

@test "exit 0 on successful query" {
  run "$TRILOG" -e "true."
  [ "$status" -eq 0 ]
}

@test "exit 0 on failed query (query failure is not process error)" {
  run "$TRILOG" -e "false."
  [ "$status" -eq 0 ]
}

@test "exit 1 on parse error" {
  run "$TRILOG" -e "garbage@@."
  [ "$status" -eq 1 ]
}

@test "exit 0 on thrown exception" {
  run "$TRILOG" -e "throw(boom)."
  [ "$status" -eq 0 ]
  [[ "$output" == *"boom"* ]]
}

# --- file loading (-f) ---

@test "-f loads clauses and -e can query them" {
  run "$TRILOG" -f test/family.pl -e "parent(tom,X), write(X)."
  [ "$status" -eq 0 ]
  [[ "$output" == *"bob"* ]]
}

@test "-f with nonexistent file exits nonzero" {
  run "$TRILOG" -f nonexistent_file.pl -e "true."
  [ "$status" -ne 0 ]
}

# --- pipe (non-interactive) mode ---

@test "pipe mode: query via stdin" {
  result=$(echo "append([1],[2],X)." | "$TRILOG" 2>&1)
  [[ "$result" == *"X = [1, 2]"* ]]
}

@test "pipe mode: no prompt in output" {
  result=$(echo "true." | "$TRILOG" 2>&1)
  [[ "$result" != *"?-"* ]]
}

@test "pipe mode: multiple queries" {
  result=$(printf "write(hello).\nwrite(world).\n" | "$TRILOG" 2>&1)
  [[ "$result" == *"hello"* ]]
  [[ "$result" == *"world"* ]]
}

# --- quad runner (-q) ---

@test "-q exits 0 on passing tests" {
  run "$TRILOG" -q test/core_quad.pl
  [ "$status" -eq 0 ]
}

@test "-q exits 1 on failing test" {
  echo '?- 1 =:= 2.' > /tmp/trilog_fail_quad.pl
  echo '   true.' >> /tmp/trilog_fail_quad.pl
  run "$TRILOG" -q /tmp/trilog_fail_quad.pl
  [ "$status" -eq 1 ]
  rm -f /tmp/trilog_fail_quad.pl
}

# --- error messages ---

@test "type error prints meaningful message" {
  run "$TRILOG" -e "X is hello."
  [[ "$output" == *"type_error"* ]]
}

@test "existence error for unknown predicate" {
  run "$TRILOG" -e "nonexistent_pred(1,2,3)."
  [[ "$output" == *"existence_error"* ]]
}

@test "instantiation error on unbound arithmetic" {
  run "$TRILOG" -e "X is Y + 1."
  [[ "$output" == *"instantiation_error"* ]]
}

# --- multiple expressions in sequence ---

@test "assert persists across queries in pipe" {
  result=$(printf "assert(color(red)).\ncolor(X), write(X).\n" | "$TRILOG" 2>&1)
  [[ "$result" == *"red"* ]]
}

# --- consult via -e ---

@test "consult and query in one expression" {
  run "$TRILOG" -e "consult('test/family.pl'), parent(tom,bob)."
  [ "$status" -eq 0 ]
}

# --- STR type consistency ---

@test "string unification" {
  run "$TRILOG" -e "X = \"hello\", X = [h,e,l,l,o], write(ok)."
  [ "$status" -eq 0 ]
  [[ "$output" == *"ok"* ]]
}

@test "string in arithmetic context gives type error" {
  run "$TRILOG" -e "X is \"hello\"."
  [[ "$output" == *"type_error"* ]]
}

# --- edge cases ---

@test "empty expression" {
  run "$TRILOG" -e ""
  # should not crash
  true
}

@test "deeply nested term doesn't crash" {
  run "$TRILOG" -e "X = f(f(f(f(f(f(f(f(f(f(a)))))))))), write(ok)."
  [ "$status" -eq 0 ]
  [[ "$output" == *"ok"* ]]
}

@test "long atom name doesn't crash" {
  run "$TRILOG" -e "X = abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz, write(ok)."
  [ "$status" -eq 0 ]
  [[ "$output" == *"ok"* ]]
}
