% solver tests

% --- append ---

?- append([], [], X).
   X = [].

?- append([], [1, 2], X).
   X = [1, 2].

?- append([1, 2], [], X).
   X = [1, 2].

?- append([1, 2], [3, 4], X).
   X = [1, 2, 3, 4].

?- append([a], [b], X).
   X = [a, b].

?- append([1, 2], [3], [1, 2, 3]).
   true.

?- append([1, 2], [3], [1, 2, 4]).
   false.

% --- member ---

?- member(1, [1, 2, 3]).
   true.

?- member(2, [1, 2, 3]).
   true.

?- member(3, [1, 2, 3]).
   true.

?- member(4, [1, 2, 3]).
   false.

?- member(1, []).
   false.

?- member(a, [a]).
   true.

?- member(b, [a]).
   false.

% --- combined ---

?- append([1], [2], X), member(1, X).
   X = [1, 2].

?- append([1], [2], X), member(3, X).
   false.

% --- facts ---

foo_s(a).
foo_s(b).

?- foo_s(a).
   true.

?- foo_s(X).
   X = a
;  X = b.

?- foo_s(c).
   false.

% --- nested structures ---

fs(g(a)).

?- fs(g(X)).
   X = a.

?- fs(h(X)).
   false.

% --- rules ---

sparent(tom, bob).
sparent(bob, jim).
sgrandparent(X, Z) :- sparent(X, Y), sparent(Y, Z).

?- sparent(tom, X).
   X = bob.

?- sgrandparent(tom, X).
   X = jim.

% --- anonymous variables ---

safoo(a, b).

?- safoo(_, b).
   true.

?- safoo(_, _).
   true.

sapair(a, b).

?- sapair(_, _).
   true.

safirst([H|_], H).

?- safirst([1, 2, 3], X).
   X = 1.

sasecond([_, X|_], X).

?- sasecond([a, b, c], X).
   X = b.

?- _ = foo.
   true.

sawrap(f(a, b)).

?- sawrap(_).
   true.

safoo2(a).
sabar :- safoo2(_).

?- sabar.
   true.

safoo3(a).
safoo3(b).
saboth :- safoo3(_), safoo3(_).

?- once(saboth).
   true.
