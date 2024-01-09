shell :- 
    write('Next command ?- '),nl,  read(G), shell(G).

shell(exit) :- !.

shell(G) :-
    ground(G), !, shell_solve_ground(G), shell.

shell(G) :-
    shell_solve(G), shell.

shell_solve(G) :- 
    G, write(G), nl, fail.

shell_solve(_) :-
    write('No (more) solutions'), nl.

shell_solve_ground(G) :- 
    G, !, write('Yes'), nl.

shell_solve_ground(_) :- write('No'), nl.

% ?- shell.
% ?- between(1,10,I). %prints all solutions
