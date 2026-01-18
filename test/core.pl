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