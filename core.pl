% reference: https://www.complang.tuwien.ac.at/ulrich/iso-prolog/prologue#status_quo

append([], L, L).
append([H|T], L, [H|R]) :- append(T, L, R).

member(X, [X|_]).
member(X, [_|T]) :- member(X, T).

length(Xs, N) :-
    (  var(N)
    -> length_acc(Xs, 0, N)
    ;  (  integer(N), N >= 0
       -> length_create(N, Xs)
       ;  (  integer(N)
          -> throw(error(domain_error(not_less_than_zero, N), length/2))
          ;  throw(error(type_error(integer, N), length/2))
          )
       )
    ).

length_acc([], N, N).
length_acc([_|Xs], N0, N) :-
    N1 is N0 + 1,
    length_acc(Xs, N1, N).

length_create(0, []) :- !.
length_create(N, [_|Xs]) :-
    N > 0,
    N1 is N - 1,
    length_create(N1, Xs).

reverse(L, R) :- reverse(L, [], R).

reverse([], Acc, Acc).
reverse([H|T], Acc, R) :- reverse(T, [H|Acc], R).

last(X, [X]).
last(X, [_|T]) :- last(X, T).

between(Low, High, Low) :- Low =< High.
between(Low, High, X) :- Low < High, Low1 is Low + 1, between(Low1, High, X).

once(Goal) :- call(Goal), !.

repeat.
repeat :- repeat.

false :- fail.

succ(X, S) :-
    (  integer(X)
    -> S is X + 1
    ;  integer(S), S > 0
    -> X is S - 1
    ).

plus(A, B, C) :-
    (  integer(A), integer(B)
    -> C is A + B
    ;  integer(A), integer(C)
    -> B is C - A
    ;  integer(B), integer(C)
    -> A is C - B
    ).

forall_fail(Cond, Action) :- call(Cond), \+ call(Action).
forall(Cond, Action) :- \+ forall_fail(Cond, Action).

select(E, [E|Xs], Xs).
select(E, [X|Xs], [X|Ys]) :-
    select(E, Xs, Ys).

nth0(N, List, Elem) :-
    (  integer(N)
    -> N >= 0, nth0_det(N, List, Elem)
    ;  nth0_gen(0, List, N, Elem)
    ).

nth0_det(0, [E|_], E) :- !.
nth0_det(N, [_|Xs], E) :- N1 is N - 1, nth0_det(N1, Xs, E).

nth0_gen(N, [E|_], N, E).
nth0_gen(N0, [_|Xs], N, E) :- N1 is N0 + 1, nth0_gen(N1, Xs, N, E).

nth0(N, List, Elem, Rest) :-
    (  integer(N)
    -> N >= 0, nth0_det4(N, List, Elem, Rest)
    ;  nth0_gen4(0, List, N, Elem, Rest)
    ).

nth0_det4(0, [E|Xs], E, Xs) :- !.
nth0_det4(N, [X|Xs], E, [X|Ys]) :- N1 is N - 1, nth0_det4(N1, Xs, E, Ys).

nth0_gen4(N, [E|Xs], N, E, Xs).
nth0_gen4(N0, [X|Xs], N, E, [X|Ys]) :- N1 is N0 + 1, nth0_gen4(N1, Xs, N, E, Ys).

nth1(N, Es0, E) :- nth1(N, Es0, E, _).

nth1(N, List, Elem, Rest) :-
    (  integer(N)
    -> N >= 1, N0 is N - 1, nth0_det4(N0, List, Elem, Rest)
    ;  nth0_gen4(0, List, N0, Elem, Rest), N is N0 + 1
    ).

maplist(_R_1, []).
maplist(R_1, [E1|E1s]) :-
    call(R_1, E1),
    maplist(R_1, E1s).

maplist(_R_2, [], []).
maplist(R_2, [E1|E1s], [E2|E2s]) :-
    call(R_2, E1, E2),
    maplist(R_2, E1s, E2s).

maplist(_R_3, [], [], []).
maplist(R_3, [E1|E1s], [E2|E2s], [E3|E3s]) :-
    call(R_3, E1, E2, E3),
    maplist(R_3, E1s, E2s, E3s).

foldl(_, [], S, S).
foldl(R_3, [X|Xs], S0, S) :-
    call(R_3, X, S0, S1),
    foldl(R_3, Xs, S1, S).

foldl(_, [], [], S, S).
foldl(R_4, [X|Xs], [Y|Ys], S0, S) :-
    call(R_4, X, Y, S0, S1),
    foldl(R_4, Xs, Ys, S1, S).

foldl(_, [], [], [], S, S).
foldl(R_5, [X|Xs], [Y|Ys], [Z|Zs], S0, S) :-
    call(R_5, X, Y, Z, S0, S1),
    foldl(R_5, Xs, Ys, Zs, S1, S).

countall(Goal, Count) :-
    findall(_, Goal, Xs),
    length(Xs, Count).

current_op(Priority, Type, Name) :-
    current_op_count(N),
    N1 is N - 1,
    between(0, N1, I),
    current_op_entry(I, Priority, Type, Name).