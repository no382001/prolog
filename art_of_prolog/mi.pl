solve(true) :- !. 
% prevent backtracking and fix: No permission to access private_procedure `true/0'
solve((A,B)) :- solve(A), solve(B).
solve(A) :- clause(A, B), solve(B).

a(1).

solve(a(1),true).
solve(a(2),false).

%solve(Goal,Tree) :- proof tree goal
solve(true,true).
solve((A,B),(PA,PB)) :-
    solve(A,PA), solve(B,PB).
solve(A,A :- P) :- % A :- P wtf
    clause(A,B), solve(B,P).

/*
    ?- solve(a(X),T).
X = 1,
T = true ;
X = 2,
T = false ;
X = 1,
T =  (a(1):-true) ;

*/
