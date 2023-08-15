/*
An Introduction to Prolog Programming
Ulle Endriss
 */

% facts
bigger(elephant,horse).
bigger(horse, donkey).
bigger(donkey, dog).
bigger(donkey, monkey).

% transitive closure
is_bigger(X, Y) :- bigger(X, Y).
is_bigger(X, Y) :- bigger(X, Z), is_bigger(Z, Y).

mortal(X) :- man(X).
man(socrates).
man(platon). % if you ask, man(_something) it will print all values corresponding

% excercise 1.3
female(mary).
female(sandra).
female(juliet).
female(lisa).
male(peter).
male(paul).
male(dick).
male(bob).
male(harry).
parent(bob, lisa).
parent(bob, paul).
parent(bob, mary).
parent(juliet, lisa).
parent(juliet, paul).
parent(juliet, mary).
parent(peter, harry).
parent(lisa, harry).
parent(mary, dick).
parent(mary, sandra).
/*
define relations:
(a) father
(b) sister
(c) grandmother
(d) cousin
*/
father(Of,To) :- male(Of), parent(Of,To).

% why does it yield true twice tho?
sister(X,Y) :- female(X), parent(Z,X), parent(Z,X), X \= Y. 

grandmother(X,Y) :- female(X), parent(Z,Y), parent(X,Z).