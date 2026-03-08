% error tests

% --- is/2 instantiation errors ---

?- X is Y.
   error(instantiation_error).

?- X is Y + 1.
   error(instantiation_error).

?- X is 2 * (Y + 3).
   error(instantiation_error).

% --- arithmetic comparison instantiation errors ---

?- X < 3.
   error(instantiation_error).

?- 3 < X.
   error(instantiation_error).

?- X > 3.
   error(instantiation_error).

?- X =< 3.
   error(instantiation_error).

?- X >= 3.
   error(instantiation_error).

?- X =:= 3.
   error(instantiation_error).

?- X =\= 3.
   error(instantiation_error).

% --- functor/3 instantiation errors ---

?- functor(X, Y, Z).
   error(instantiation_error).

?- functor(X, Y, 2).
   error(instantiation_error).

?- functor(X, foo, Z).
   error(instantiation_error).

% --- functor/3 still works when properly called ---

?- functor(foo(a, b), N, A).
   N = foo, A = 2.

?- functor(T, foo, 2), T = foo(a, b).
   T = foo(a, b).

% --- error propagation ---

?- once(X is Y).
   error(instantiation_error).

?- \+ (X is Y).
   error(instantiation_error).

% --- error does not bleed into next query ---

?- X is Y.
   error(instantiation_error).

?- 1 < 2.
   true.

% --- bound variable does not error ---

?- Y = 5, X is Y + 1.
   Y = 5, X = 6.

?- Y = 3, Y < 5.
   Y = 3.
