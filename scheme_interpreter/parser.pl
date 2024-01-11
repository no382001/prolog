:- use_module(library(pio)).
:- use_module(library(dcg/basics)).

expressions([E|Es]) -->
    ws, expression(E), ws, !, expressions(Es).
expressions([]) --> [].

ws --> [W], { char_type(W, white) ; W = '\n' }, !, ws.
ws --> [].

expression(s(A)) --> symbol(Cs), { atom_chars(A, Cs) }.
expression(n(N)) --> number(N).
expression(L) --> ['('], expressions(L), !, [')'].
expression([s(quote),Q]) --> ['\''], expression(Q).

symbol([A|As]) -->
    [A], { memberchk(A, [+,/,-,*,>,<,=]) ; char_type(A, alpha) }, symbolr(As).

symbolr([A|As]) -->
    [A], { char_type(A, alnum) }, symbolr(As).
symbolr([]) --> [].


/*
?- string_chars("(+ 1 1)",Cs),expressions(Es,Cs,[]).
Cs = ['(', +, ' ', '1', ' ', '1', ')'],
Es = [[s(+), n(1), n(1)]].

?- string_chars("'(+ 1 1)",Cs),expressions(Es,Cs,[]).
Cs = ['\'', '(', +, ' ', '1', ' ', '1', ')'],
Es = [[s(quote), [s(+), n(1), n(1)]]].
*/