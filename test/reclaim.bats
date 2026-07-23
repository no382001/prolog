#!/usr/bin/env bats

TRILOG="./trilog"

get_stat() {
  local key="$1" output="$2"
  echo "$output" | grep "^${key}=" | cut -d= -f2
}

@test "baseline: core.pl loads without error" {
  run "$TRILOG" -s -e "true."
  [ "$status" -eq 0 ]
  perm=$(get_stat perm_pool "$output")
  [ "$perm" -gt 0 ]
}

@test "unconsult reclaims perm pool" {
  baseline=$("$TRILOG" -s -e "true." 2>&1 1>/dev/null)
  before=$(get_stat perm_pool "$baseline")

  after_out=$("$TRILOG" -s -e "consult('lib/ledit.pl'), unconsult('lib/ledit.pl')." 2>&1 1>/dev/null)
  after=$(get_stat perm_pool "$after_out")

  [ "$after" -eq "$before" ]
}

@test "double consult/unconsult cycle: no growth" {
  once=$("$TRILOG" -s -e "consult('lib/ledit.pl'), unconsult('lib/ledit.pl')." 2>&1 1>/dev/null)
  p1=$(get_stat perm_pool "$once")

  twice=$("$TRILOG" -s -e "consult('lib/ledit.pl'), unconsult('lib/ledit.pl'), consult('lib/ledit.pl'), unconsult('lib/ledit.pl')." 2>&1 1>/dev/null)
  p2=$(get_stat perm_pool "$twice")

  [ "$p2" -eq "$p1" ]
}

@test "clause count restored after unconsult" {
  baseline=$("$TRILOG" -s -e "true." 2>&1 1>/dev/null)
  before=$(get_stat clauses "$baseline")

  after_out=$("$TRILOG" -s -e "consult('lib/ledit.pl'), unconsult('lib/ledit.pl')." 2>&1 1>/dev/null)
  after=$(get_stat clauses "$after_out")

  [ "$after" -eq "$before" ]
}

@test "core predicates survive consult/unconsult" {
  run "$TRILOG" -e "consult('lib/ledit.pl'), unconsult('lib/ledit.pl'), append([1,2],[3,4],X), write(X)."
  [ "$status" -eq 0 ]
  [[ "$output" == *"[1, 2, 3, 4]"* ]]
}

@test "core predicates survive double cycle" {
  run "$TRILOG" -e "consult('lib/ledit.pl'), unconsult('lib/ledit.pl'), consult('lib/ledit.pl'), unconsult('lib/ledit.pl'), findall(X, member(X,[a,b,c]), L), write(L)."
  [ "$status" -eq 0 ]
  [[ "$output" == *"abc"* ]]
}

@test "assert/retract cycle after unconsult" {
  run "$TRILOG" -e "consult('lib/ledit.pl'), unconsult('lib/ledit.pl'), assert(tmp(1)), assert(tmp(2)), retract(tmp(1)), tmp(X), write(X)."
  [ "$status" -eq 0 ]
  [[ "$output" == *"2"* ]]
}

@test "string pool does not grow on re-consult" {
  once=$("$TRILOG" -s -e "consult('lib/ledit.pl'), unconsult('lib/ledit.pl')." 2>&1 1>/dev/null)
  s1=$(get_stat string_pool "$once")

  twice=$("$TRILOG" -s -e "consult('lib/ledit.pl'), unconsult('lib/ledit.pl'), consult('lib/ledit.pl'), unconsult('lib/ledit.pl')." 2>&1 1>/dev/null)
  s2=$(get_stat string_pool "$twice")

  [ "$s2" -eq "$s1" ]
}
