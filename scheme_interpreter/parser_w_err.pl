:- use_module(library(pio)).
:- use_module(library(dcg/basics)).

expressions([E|Es]) -->
    ws, expression(E), ws, !, expressions(Es).
expressions([]) --> [].

ws --> [W], { char_type(W, white) }, !, ws.
ws --> [].

expression(s(A)) --> symbol(Cs), { atom_chars(A, Cs) }.
expression(n(N)) --> number(N).
expression(L) --> ['('], expressions(L), [')'], !.
expression(syntax_error=open_braces_in(L)) --> ['('], expressions(L), !.
expression([s(quote),Q]) --> ['\''], expression(Q).

symbol([A|As]) -->
    [A], { memberchk(A, [+,/,-,*,>,<,=]) ; char_type(A, alpha) }, symbolr(As).

symbolr([A|As]) -->
    [A], { memberchk(A, [+,/,-,*,>,<,=]) ; char_type(A, alnum) }, symbolr(As).
symbolr([]) --> [].


main :- 
    % syntax error open braces
    string_chars("(+ 1(+ 1 1 1(+ 1 1))",Cs),expressions(Es,Cs,[]),
    Es =  [syntax_error=open_braces_in(_)],
    % syntax is fine
    string_chars("(+ 1(+ 1 1 1(+ 1) 1))",Cs1),expressions(_Es1,Cs1,[]),
    % multiple expressions on top level
    string_chars("(+ 1 11) (+ 1 1)",Cs2),expressions(_Es2,Cs2,[]).
    % syntax error in symbol name
    %string_chars("(2as 1 1)",Cs3),expressions(_Es3,Cs3,[]).

    %string_chars("(2as 1 1)",Cs3),expressions(_Es3,Cs3,[]).

/*
string_chars("(2as 1 1)",Cs3),expressions(_Es3,Cs3,[]).
Cs3 = ['(', '2', a, s, ' ', '1', ' ', '1', ')'],
_Es3 = [[n(2), s(as), n(1), n(1)]].

stricter syntax, mandatory whitespaces between symbols and numbers
*/
