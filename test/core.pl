append([], L, L).
append([H|T], L, [H|R]) :- append(T, L, R).

member(X, [X|_]).
member(X, [_|T]) :- member(X, T).

length([], 0).
length([_|T], N) :- length(T, N1), N is N1 + 1.

reverse([], []).
reverse([H|T], R) :- reverse(T, RT), append(RT, [H], R).

last(X, [X]).
last(X, [_|T]) :- last(X, T).

perm([], []).
perm([H|T], P) :- perm(T, PT), insert(H, PT, P).

insert(X, L, [X|L]).
insert(X, [H|T], [H|R]) :- insert(X, T, R).

between(Low, High, Low) :- Low =< High.
between(Low, High, X) :- Low < High, Low1 is Low + 1, between(Low1, High, X).

once(Goal) :- call(Goal), !.

forall_fail(Cond, Action) :- call(Cond), \+ call(Action).
forall(Cond, Action) :- \+ forall_fail(Cond, Action).

fib(0, 0).
fib(1, 1).
fib(N, F) :-
    N > 1,
    N1 is N - 1,
    N2 is N - 2,
    fib(N1, F1),
    fib(N2, F2),
    F is F1 + F2.