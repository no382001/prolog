% List Manipulation

% ?- concat_lists(X,Y,[a,b,c,d]).
% will show all permutations of how the list could come together
% concat_list/3 is same as append/3
concat_lists([],List,List).
concat_lists([X|Xs],List,[X|Ys])
    :- concat_lists(Xs,List,Ys).

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
remove_duplicates([], []).
remove_duplicates([X|Xs], [X|Ys]) :-
    \+ member(X, Xs),
    remove_duplicates(Xs, Ys).
remove_duplicates([X|Xs], Ys) :-
    member(X, Xs),
    remove_duplicates(Xs, Ys).

% returns the list backwards, but is one function
% '(' is a subgoal?
rd2([], []).
rd2([X|Xs], UniqueList) :-
    rd2(Xs, List),
    (   member(X, List)
    ->  UniqueList = List
    ;   UniqueList = [X|List]
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

replace([],_,_,_).

replace([Value|Xs],Value,Subst,_) :-
    replace(Xs,Value,Subst,[Subst|Xs]).
    
replace([X|Xs],Value,Subst,_) :-
    replace(Xs,Value,Subst,[X|Xs]). % pass over