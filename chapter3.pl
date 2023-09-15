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