% cut tests

% --- cut prevents backtracking ---

ca(1).
ca(2).
ca(3).
cfirst(X) :- ca(X), !.

?- once(cfirst(X)).
   X = 1.

% --- cut in middle of clause ---

cb(1).
cb(2).
ctest(X, Y) :- ca(X), !, cb(Y).

?- ctest(1, Y).
   Y = 1
;  Y = 2.

% --- cut must prune the caller's own choice point, not just the choice
% point of a multi-clause subgoal called with an already-bound argument
% (regression: first-argument indexing must deref the goal's argument to see
% that binding, otherwise a spurious choice point gets pushed for the
% subgoal and steals the cut, leaving the caller's alternative clause live) ---

cset(x).
cset(y).
cdispatch(X, matched) :- cset(X), !.
cdispatch(_, fallback).

?- findall(R, cdispatch(x, R), L).
   L = [matched].
