% reference: https://www.complang.tuwien.ac.at/ulrich/iso-prolog/prologue#status_quo

append([], L, L).
append([H|T], L, [H|R]) :- append(T, L, R).

member(X, [X|_]).
member(X, [_|T]) :- member(X, T).

must_be(Type, Term) :-
    (  var(Type)
    -> throw(error(instantiation_error, must_be/2))
    ;  must_be_(Type, Term)
    ).

must_be_(var, Term) :-
    (  var(Term)
    -> true
    ;  throw(error(uninstantiation_error(Term), must_be/2))
    ).
must_be_(atom, Term)               :- must_be_type(atom, Term).
must_be_(integer, Term)            :- must_be_type(integer, Term).
must_be_(number, Term)             :- must_be_type(number, Term).
must_be_(atomic, Term)             :- must_be_type(atomic, Term).
must_be_(callable, Term)           :- must_be_type(callable, Term).
must_be_(compound, Term)           :- must_be_type(compound, Term).
must_be_(boolean, Term)            :- must_be_type(boolean, Term).
must_be_(character, Term)          :- must_be_type(character, Term).
must_be_(list, Term)               :- must_be_list(Term).
must_be_(not_less_than_zero, Term) :-
    must_be_type(integer, Term),
    (  Term >= 0
    -> true
    ;  throw(error(domain_error(not_less_than_zero, Term), must_be/2))
    ).

must_be_type(Type, Term) :-
    (  var(Term)
    -> throw(error(instantiation_error, must_be/2))
    ;  call(Type, Term)
    -> true
    ;  throw(error(type_error(Type, Term), must_be/2))
    ).

must_be_list([]) :- !.
must_be_list([_|T]) :- !, must_be_list(T).
must_be_list(Term) :-
    (  var(Term)
    -> throw(error(instantiation_error, must_be/2))
    ;  throw(error(type_error(list, Term), must_be/2))
    ).

boolean(true).
boolean(false).

character(C) :- atom(C), atom_length(C, 1).

can_be(Type, Term) :-
    (  var(Type)
    -> throw(error(instantiation_error, can_be/2))
    ;  var(Term)
    -> true
    ;  must_be(Type, Term)
    ).

length(Xs, N) :-
    (  var(N)
    -> length_acc(Xs, 0, N)
    ;  must_be(not_less_than_zero, N),
       length_create(N, Xs)
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

current_prolog_flag(Flag, Value) :-
    (   nonvar(Flag)
    ->  prolog_flag_value(Flag, Value)
    ;   member(Flag, [bounded, max_integer, min_integer,
                      integer_rounding_function, max_arity, double_quotes]),
        prolog_flag_value(Flag, Value)
    ).

current_op(Priority, Type, Name) :-
    current_op_count(N),
    N1 is N - 1,
    between(0, N1, I),
    current_op_entry(I, Priority, Type, Name).

compare(Order, A, B) :-
    (  A == B
    -> Order = (=)
    ;  A @< B
    -> Order = (<)
    ;  Order = (>)
    ).

memberchk(X, L) :- member(X, L), !.

delete([], _, []).
delete([X|Xs], Y, Zs) :-
    \+ X \= Y,
    !,
    delete(Xs, Y, Zs).
delete([X|Xs], Y, [X|Zs]) :-
    delete(Xs, Y, Zs).

include(_, [], []).
include(P_1, [X|Xs], Included) :-
    (  call(P_1, X)
    -> Included = [X|Included1]
    ;  Included = Included1
    ),
    include(P_1, Xs, Included1).

exclude(_, [], []).
exclude(P_1, [X|Xs], Excluded) :-
    (  call(P_1, X)
    -> Excluded = Excluded1
    ;  Excluded = [X|Excluded1]
    ),
    exclude(P_1, Xs, Excluded1).

partition(_, [], [], []).
partition(P_1, [X|Xs], Included, Excluded) :-
    (  call(P_1, X)
    -> Included = [X|Included1], Excluded = Excluded1
    ;  Included = Included1, Excluded = [X|Excluded1]
    ),
    partition(P_1, Xs, Included1, Excluded1).

subtract([], _, []).
subtract([X|Xs], Ys, Zs) :-
    (  memberchk(X, Ys)
    -> subtract(Xs, Ys, Zs)
    ;  Zs = [X|Zs1], subtract(Xs, Ys, Zs1)
    ).

intersection([], _, []).
intersection([X|Xs], Ys, Zs) :-
    (  memberchk(X, Ys)
    -> Zs = [X|Zs1]
    ;  Zs = Zs1
    ),
    intersection(Xs, Ys, Zs1).

union([], L, L).
union([X|Xs], Ys, Zs) :-
    (  memberchk(X, Ys)
    -> union(Xs, Ys, Zs)
    ;  Zs = [X|Zs1], union(Xs, Ys, Zs1)
    ).

sum_list(L, Sum) :- sum_list_(L, 0, Sum).
sum_list_([], S, S).
sum_list_([X|Xs], S0, S) :- S1 is S0 + X, sum_list_(Xs, S1, S).

max_list([X|Xs], Max) :- max_list_(Xs, X, Max).
max_list_([], M, M).
max_list_([X|Xs], M0, M) :-
    (  X > M0 -> M1 = X ; M1 = M0 ),
    max_list_(Xs, M1, M).

min_list([X|Xs], Min) :- min_list_(Xs, X, Min).
min_list_([], M, M).
min_list_([X|Xs], M0, M) :-
    (  X < M0 -> M1 = X ; M1 = M0 ),
    min_list_(Xs, M1, M).

max_member(Max, [X|Xs]) :- foldl(max_member_, Xs, X, Max).
max_member_(X, M0, M) :- ( X @> M0 -> M = X ; M = M0 ).

min_member(Min, [X|Xs]) :- foldl(min_member_, Xs, X, Min).
min_member_(X, M0, M) :- ( X @< M0 -> M = X ; M = M0 ).

numlist(Low, High, []) :- Low > High, !.
numlist(Low, High, [Low|Rest]) :-
    Low =< High,
    Low1 is Low + 1,
    numlist(Low1, High, Rest).

flatten(List, FlatList) :-
    flatten_(List, [], FlatList0),
    FlatList = FlatList0.

flatten_(Var, Tl, [Var|Tl]) :- var(Var), !.
flatten_([], Tl, Tl) :- !.
flatten_([Hd|Tl], Tail, List) :-
    !,
    flatten_(Hd, FlatHeadTail, List),
    flatten_(Tl, Tail, FlatHeadTail).
flatten_(NonList, Tl, [NonList|Tl]).

list_to_set(List, Set) :- list_to_set_(List, [], Set).
list_to_set_([], _, []).
list_to_set_([X|Xs], Seen, Set) :-
    (  memberchk(X, Seen)
    -> list_to_set_(Xs, Seen, Set)
    ;  Set = [X|Set1],
       list_to_set_(Xs, [X|Seen], Set1)
    ).

permutation([], []).
permutation(List, [X|Perm]) :-
    select(X, List, Rest),
    permutation(Rest, Perm).