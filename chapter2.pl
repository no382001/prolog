% List Manipulation

% ?- concat_lists(X,Y,[a,b,c,d]).
% will show all permutations of how the list could come together
% concat_list/3 is same as append/3
concat_lists([],List,List).
concat_lists([X|Xs],List,[X|Ys])
    :- concat_lists(Xs,List,Ys). %when Xs runs out List gets list gets matched

isthreelist(List) :- length(List,X), X = 3.
% likewise, used with a number in place of X, and an unused variable,
% length/2 will generate a list of free variables of the len 3

%?- member(dog, [elephant, horse, donkey, dog, monkey]).

% last/2
% reverse/2
% select/3

/* Exercise 2.1. Write a Prolog predicate analyse_list/1 that takes a list as its argu-
ment and prints out the list’s head and tail on the screen.
If the given list is empty, the
predicate should put out an message reporting this fact. If the argument term isn’t a
list at all, the predicate should just fail.
*/

analyse_list(_) :- fail.
analyse_list([]) :- write('list is empty').
analyse_list([X|Xs])
    :- write('head: '), write(X),
       write('\ntail: '), write(Xs).


/* Exercise 2.2. Write a Prolog predicate membership/2 that works like the built-in
predicate member/2 (without using member/2).
*/
membership(X,[X|_]).
membership(X,[_|Ys])
    :- membership(X,Ys).

/* Exercise 2.3. Implement a Prolog predicate remove_duplicates/2 that removes all
duplicate elements from a list given in the first argument and returns the result in the
second argument position. Example:

    ?- remove_duplicates([a, b, a, c, d, d], List).
    List = [b, a, c, d]
    Yes  

*/
% \+ is true if it cannot be proven

remove_duplicates([], []).
remove_duplicates([X|Xs], [X|Ys]) :-
    \+ member(X, Xs),
    remove_duplicates(Xs, Ys).
remove_duplicates([X|Xs], Ys) :-
    member(X, Xs),
    remove_duplicates(Xs, Ys).

% returns the list backwards, but is one function
% '(' is a subgoal? its basically if then else
rd2([], []).
rd2([X|Xs], UniqueList) :-
    rd2(Xs, List),
    (   member(X, List)
    ->  UniqueList = List
    ;   UniqueList = [X|List]
    ).

% this does not reverse the order
remove_duplicates_acc_ver(Input, Output) :-
    remove_duplicates_acc(Input, [], ReversedOutput),
    reverse(ReversedOutput, Output).

remove_duplicates_acc([], Acc, Acc).
remove_duplicates_acc([X|Xs], Acc, Output) :-
    (   member(X, Acc)
    ->  remove_duplicates_acc(Xs, Acc, Output)
    ;   remove_duplicates_acc(Xs, [X|Acc], Output)
    ).


/* Exercise 2.4. Write a Prolog predicate reverse_list/2 that works like the built-in
predicate reverse/2 (without using reverse/2). Example:

    ?- reverse_list([tiger, lion, elephant, monkey], List).
    List = [monkey, elephant, lion, tiger]
    Yes
*/

% tail recursion is needed otherwise it never terminates, append cannot be on the front
reverse_list([], []).
reverse_list([X|Xs], Ys) :-
    reverse_list(Xs, Zs),
    append(Zs, [X], Ys).


/* Exercise 2.6. The objective of this exercise is to implement a predicate for returning
the last element of a list in two different ways.
(a) Write a predicate last1/2 that works like the built-in predicate last/2 using a
recursion and the head/tail-pattern for lists.
(b) Define a similar predicate last2/2 solely in terms of append/3, without using a
recursion. */

%(a)
last1([X],X).
last1([_|Xs],Last) :-
    last1(Xs,Last).

%(b)
% reverse substitution? Xs's last element gets substituted into Last
% same happened with concat_list/3
last2(Xs,Last) :- append(_,[Last],Xs).



/* Exercise 2.7. Write a predicate replace/4 to replace all occurrences of a given ele-
ment (second argument) by another given element (third argument) in a given list (first
argument). Example:
    ?- replace([1, 2, 3, 4, 3, 5, 6, 3], 3, x, List).
    List = [1, 2, x, 4, x, 5, 6, x]
    Yes
*/

replace([],_,_,[]).

replace([Value|Xs],Value,Subst,[Subst|Ys]) :-
    replace(Xs, Value, Subst, Ys).

replace([X|Xs], Value, Subst, [X|Ys]) :-
    X \= Value,
    replace(Xs, Value, Subst, Ys).


replace_one_body([], _, _, []).

replace_one_body([X|Xs], Value, Subst, [Y|Ys]) :-
    (X = Value
    -> Y = Subst
    ; Y = X),
    replace_one_body(Xs, Value, Subst, Ys).

/*
if 'X \= Value,' is removed, Prolog will explore the variations in which something can be replaced,
it loses the ability to tell what should be replaced
*/


/* Exercise 2.8. Prolog lists without duplicates can be interpreted as sets. Write a
program that given such a list computes the corresponding power set. Recall that the
power set of a set S is the set of all subsets of S. This includes the empty set as well as
the set S itself.
Define a predicate power/2 such that, if the first argument is instantiated with a
list, the corresponding power set (i.e., a list of lists) is returned in the second position.
Example:
    ?- power([a, b, c], P).
    P = [[a, b, c], [a, b], [a, c], [a], [b, c], [b], [c], []]
    Yes
Note: The order of the sub-lists in your result doesn’t matter.
*/

powerset([], []).
powerset([H|T], [H|P]) :- powerset(T,P).
powerset([_|T], P) :- powerset(T,P).

/*
[trace]  ?- powerset([a,b,c],L).
   Call: (10) powerset([a, b, c], _240) ? creep
   Call: (11) powerset([b, c], _642) ? creep
   Call: (12) powerset([c], _692) ? creep
   Call: (13) powerset([], _742) ? creep
   Exit: (13) powerset([], []) ? creep
   Exit: (12) powerset([c], [c]) ? creep
   Exit: (11) powerset([b, c], [b, c]) ? creep
   Exit: (10) powerset([a, b, c], [a, b, c]) ? creep
L = [a, b, c] .
*/