% regression: for 0'c parsing and dcg

?- has_complete_clause('?- number_codes(33.0, [0''3|_L]).').
   true.

?- has_complete_clause('?- X = 0''\\n.').
   true.

?- has_complete_clause('?- X is 0''\\\\ + 1.').
   true.

?- has_complete_clause('?- foo([0''a, 0''b], X).').
   true.

?- has_complete_clause('?- X = 0''a, Y = ''real atom''.').
   true.

?- \+ has_complete_clause('?- X = ''unterminated atom.').
   true.

?- strip_terminating_dot('X is 0''a.', Y).
   Y = 'X is 0\'a'.

?- strip_terminating_dot('X is 0''a', Y).
   Y = 'X is 0\'a'.

?- strip_line_comment('X is 0''a. % a comment', Y).
   Y = 'X is 0\'a. '.

% regression: assertz-ing a rule whose body contains an integer literal,
% then leaving it live in the database past the end of the query, used to
% corrupt the perm pool on the next compaction (substitute/3 didn't copy
% INT terms into the perm pool, so the literal stayed pointing into the
% temp pool and became a dangling pointer once things were compacted)

?- assertz((dyn_gt2(X) :- X > 2)), dyn_gt2(3).
   true.

?- dyn_gt2(5).
   true.

% regression: first-argument indexing must deref the goal's argument to see
% an already-bound value, otherwise a spurious choice point gets pushed for
% a multi-clause subgoal and steals the cut, leaving the caller's own
% alternative clause live instead of being pruned by it.

cset(x).
cset(y).
cdispatch(X, matched) :- cset(X), !.
cdispatch(_, fallback).

?- findall(R, cdispatch(x, R), L).
   L = [matched].

% regression: builtin-created bindings (is/2) must get reclaimed in step
% with the recursive clause match, not accumulate until MAX_BINDINGS.

qcount(0) :- !.
qcount(N) :- N > 0, N1 is N - 1, qcount(N1).

?- qcount(5000).
   true.

% regression: a var bound deep inside a ->'s condition (via recursion in
% another predicate) must still be valid for goals after the -> returns.

qsplit([], []).
qsplit([_|T], Rest) :- qsplit(T, Rest).
qjoin([], B, B) :- !.
qjoin([H|A], B, [H|C]) :- qjoin(A, B, C).

?- ( qsplit([x], _Suffix) -> qjoin([], "hi", _Tmp), qjoin(_Tmp, _Suffix, R) ; true ).
   R = "hi".
