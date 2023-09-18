% Operators


and(A, B) :- call(A), call(B).

or(A, B) :- call(A); call(B).

neg(A) :- \+ call(A).

implies(A, B) :- call(A), !, call(B).
implies(_, _).

neg2(A) :- call(A), !, fail.
neg2(_).


/* Exercise 5.1.
 Type the following queries into a Prolog interpreter and explain what
happens.
(a) ?- (Result = a ; Result = b), !, Result = b.
(b) ?- member(X, [a, b, c]), !, X = b.
*/

% (Result = a ; Result = b), !, Result = b.

/*
    set Result to a then set Result to b which is false,
    so skip cut, and set Result to b whichs false again 
*/

% member(X, [a, b, c]), !, X = b.
/*
    the first match for X in the list is A,
    cut and check if X is b which it is not
*/

/*
Exercise 5.2.
 Consider the following Prolog program:
    result([_, E | L], [E | M]) :- !,
        result(L, M).
    result(_, []).
(a) After having consulted this program, what would Prolog reply when presented with
the following query? Try answering this question first without actually typing in
the program, but verify your solution later on using the Prolog system.
?- result([a, b, c, d, e, f, g], X).
(b) Briefly describe what the program does and how it does what it does, when the
first argument of the result/2-predicate is instantiated with a list and a variable
is given in the second argument position, i.e., as in item (a). Your explanations
should include answers to the following questions:
– What case(s) is/are covered by the Prolog fact?
– What effect has the cut in the first line of the program?
– Why has the anonymous variable been used?
*/

result([_, E | L], [E | M]) :- !, result(L, M).
result(_, []).

/* Exercise 5.3.
Implement Euclid’s algorithm to compute the greatest common divisor
(GCD) of two non-negative integers. This predicate should be called gcd/3 and, given
two non-negative integers in the first two argument positions, should match the variable
in the third position with the GCD of the two given numbers. Examples:

    ?- gcd(57, 27, X).
    X = 3
    Yes

    ?- gcd(1, 30, X).
    X = 1
    Yes

    ?- gcd(56, 28, X).
    X = 28
    Yes

Make sure your program behaves correctly also when the semicolon key is pressed.
Hints: The GCD of two numbers a and b (with a ≥ b) can be found by recursively
substituting a with b and b with the rest of the integer division of a and b. Make sure
you define the right base case(s).
*/

/*
(define (gcd a b)
  (cond [(= b 0) a]
        [else (gcd b (modulo a b))]))
*/

gcd(X,0,X) :- X >= 0.
gcd(X,Y,Res) :- 
    X >= Y, Y > 0, 
    Mod is (X mod Y), 
    gcd(Y,Mod,Res).


% gcd(57, 27, X). %3
% gcd(1, 30, X). %1
% gcd(56, 28, X). %28

% why cant i return true on semicolon?