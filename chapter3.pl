% Arithmetic Expressions
/*
?- 3 + 5 = 8. %they do not match, first is a compound term second is a number
No

?- X is 3 + 5, X = 8.
X = 8
Yes

?- 8 is 3 + 5.
Yes

*/

/*
Exercise 3.1. Write a Prolog predicate distance/3 to calculate the distance between
two points in the 2-dimensional plane. Points are given as pairs of coordinates. Examples:
?- distance((0,0), (3,4), X).
X = 5
Yes
?- distance((-2.5,1), (3.5,-4), X).
X = 7.81025
Yes
*/

distance((X1,Y1),(X2,Y2),R) :- R is ((((X2 - X1) ** 2) + ((Y2 - Y1) ** 2)) ** 0.5).


/*
Exercise 3.2. Write a Prolog program to print out a square of n × n given characters
on the screen. Call your predicate square/2. The first argument should be a (positive)
integer, the second argument the character (any Prolog term) to be printed. Example:
?- square(5, ’*’).
* * * * *
* * * * *
* * * * *
* * * * *
* * * * *
Yes
*/
square(N,C) :-
    Total is N * N, 
    foreach(
        between(0,Total,X),
        (0 =:= X mod N
        -> print(C), nl
        ; print(C))).

square2(N, C) :-
    between(1, N, _),
    print_line(N, C),
    nl,
    fail.
square2(_, _).

print_line(0, _).
print_line(N, C) :-
    N > 0,
    print(C),
    write(' '),
    M is N - 1,
    print_line(M, C).



/*
Exercise 3.3. Write a Prolog predicate fibonacci/2 to compute the nth Fibonacci
number. The Fibonacci sequence is defined as follows:
F0 = 1
F1 = 1
Fn = Fn−1 + Fn−2 for n ≥ 2
*/

fib(N,Res) :-
    (N < 2
    -> Res is 1
    ; N1 is N - 1, N2 is N - 2, 
    fib(N1,Res1),fib(N2,Res2),
    Res is Res1 + Res2).

fib2(N,Res) :- N < 2, Res = 1.
fib2(N,Res) :-
    N1 is N - 1, N2 is N - 2, 
    fib(N1,Res1),fib(N2,Res2),
    Res is Res1 + Res2.
/*
Exercise 3.4. Write a Prolog predicate element_at/3 that, given a list and a natural
number n, will return the nth element of that list. Examples:
?- element_at([tiger, dog, teddy_bear, horse, cow], 3, X).
X = teddy_bear
Yes
?- element_at([a, b, c, d], 27, X).
No
*/

element_at([],_,_) :- fail.
element_at([X|Xs],N,Res) :- 
    (N =:= 1 
    -> Res = X
    ; Nn is N - 1, element_at(Xs,Nn,Res)).

element_at2(_,_,_) :- fail.
element_at2([X|_],1,X) :- !. % the min index
element_at2([_|Xs],N,Res) :- Nn is N - 1, element_at(Xs,Nn,Res). 

/*
Exercise 3.5. Write a Prolog predicate mean/2 to compute the arithmetic mean of a
given list of numbers. Example:
?- mean([1, 2, 3, 4], X).
X = 2.5
Yes
*/
sum([],0).
sum([H|T], S) :- sum(T,X), S is H + X.

count([],0).
count([_|T], Ss) :- count(T,S), Ss is 1 + S.

mean([],_) :- fail.
mean(List,X) :- sum(List,Total), count(List,N), X is Total / N. 

/*
Exercise 3.6. Write a predicate range/3 to generate all integers between a given lower
and a given upper bound. The lower bound should be given as the first argument, the
upper bound as the second. The result should be a list of integers, which is returned in
the third argument position. If the upper bound specified is lower than the given lower
bound, the empty list should be returned. Examples:

?- range(3, 11, X).
X = [3, 4, 5, 6, 7, 8, 9, 10, 11]
Yes
?- range(7, 4, X).
X = []
Yes
*/

range1(P,Q,[]) :- P > Q.
range1(P,Q,[P|X]) :- Pp is P + 1, range1(Pp,Q,X). 
% thinks there could be more matches, why?
% there is a conditional somewhere

range(P,Q,[]) :- P > Q, !. % add ! to prevent backtracking, but i dont think this is the solution
range(P,Q,[P|X]) :- Pp is P + 1, range(Pp,Q,X). 

/*
Exercise 3.7. Polynomials can be represented as lists of pairs of coefficients and ex-
ponents. For example the polynomial

4x5 + 2x3 − x + 27

can be represented as the following Prolog list:

[(4,5), (2,3), (-1,1), (27,0)]

Write a Prolog predicate poly_sum/3 for adding two polynomials using that representa-
tion. Try to find a solution that is independent of the ordering of pairs inside the two
given lists. Likewise, your output doesn’t have to be ordered. Examples:

?- poly_sum([(5,3), (1,2)], [(1,3)], Sum).
Sum = [(6,3), (1,2)]
Yes
?- poly_sum([(2,2), (3,1), (5,0)], [(5,3), (1,1), (10,0)], X).
X = [(4,1), (15,0), (2,2), (5,3)]
Yes

Hints: Before you even start thinking about how to do this in Prolog, recall how the
sum of two polynomials is actually computed. A rather simple solution is possible using
the built-in predicate select/3. Note that the list representation of the sum of two
polynomials that don’t share any exponents is simply the concatenation of the two lists
representing the arguments.
*/

