:- use_module(library(pio)).
:- use_module(library(dcg/basics)).

% - - - - - - - - - - - - - - - - - - - - - - - - - - - -
% --> translated to swi from sictus
% https://web.archive.org/web/20230103115618/http://aszt.inf.elte.hu/~asvanyi/pp/dcgs1p3.pdf
%   DCGS FOR PARSING AND ERROR HANDLING : TIBOR ASVANYI
%
% 4. Adding error semantics
% - - - - - - - - - - - - - - - - - - - - - - - - - - - -

:- use_module( library(lists), [reverse/2] ).

constants(Constants) -->
    constants([],Cs), {reverse(Cs,Constants)}.

% List Bs represents the semantics of the constants
% defined up to this point -- in reversed order, and
% C is the semantics of the actual constant.
constants(Bs,Cs) --> constdef(Bs,C), !, constants([C|Bs],Cs).
constants(Bs,Cs) -->
    nonempty(Tokens), !, { Cs = [error(constants,Tokens)|Bs] }.
constants(Bs,Bs) --> [].

nonempty([T|Ts]) --> [T], anything(Ts).
anything([T|Ts]) --> [T], !, anything(Ts).
anything([]) --> [].

% atom(Id) :- Id is not an error term, constant
% if Exp has an error term, exception
constdef(Bs,C) -->
    const(Bs,Id), [=], expr(Bs,Expr), ['.'], !,
    { atom(Id), catch( Val is Expr, _, fail ) -> 
        C = (Id=Val)
    ; atom(Id) -> C = error(expression,Id=Expr)
    ; catch( Val is Expr, _, fail ) ->
        C = error(constant,Id=Val)
    ; C = error(constant_and_expression,Id=Expr)
    }.

% Basic problem with the constant definition
constdef(_,error(constdef_syntax,Ts)) -->
    constdef_error(Ts).

constdef_error(['.']) --> ['.'], !.
constdef_error([X|Xs]) --> [X], constdef_error(Xs).

expr(Bs,Expr), ['.'] -->
    operand(Bs,X), operator(Op), operand(Bs,Y), ['.'], !,
    { atom(Op) -> Expr =.. [Op,X,Y] % Op is a legal operator
    ; Expr = [X,Op,Y] % Op is not a legal operator
    }.

% Basic problem with the struct of the expression:
expr(_,error(expression_syntax,Ts)) -->
    expression_error(Ts).

expression_error([]), ['.'] --> ['.'], !.
expression_error([X|Xs]) --> [X], !, expression_error(Xs).
expression_error([]) --> [].

operator(Operator) --> [Op],
    { op(Op) -> Operator = Op
    ; Operator = error(operator,Op)
    }.

op(+). op(-). op(*). op(//). op(mod).

operand(_,X) --> [Y], { integer(Y), !, X = Y }.
operand(Bs,X) --> id(Y), !,
    { member(Y=Z,Bs) -> X = Z
    ; X = error(undefined,Y)
    }.
operand(_,X) --> [Y], { X = error(operand_syntax,Y) }.

const(Bs,Id) --> id(Y), !,
    { member(Y=_,Bs) -> Id = error(redefined,Y)
    ; Id = Y
    }.
const(_,error(non_id_constant_symbol,X)) --> [X].

id(Id) --> [Id], { is_id(Id) }.

is_id(Id) :-
    atom(Id), atom_codes(Id,[C1|Cs]),
    is_lower(C1),
    \+ ( member(K,Cs), \+id_code(K) ).

id_code(K) :-
    ( is_lower(K) -> true
    ; is_upper(K) -> true
    ; is_digit(K) -> true
    ; K == 0
    ).

% symbols in id dont work
main :-
    constants(R1,[ a,=,1,+,1,.,
                aa,=,a,-,1,.,
                b,=,3,*,a,.,
                cc,=,b,//,a,.,
                d,=,b,mod,4,.],[]),
    R1 = [a=2, aa=1, b=6, cc=3, d=2],

    constants(R2,[ a,=,a,+,1,.],[]), % depends on undefined constant
    R2 = [error(expression, a=error(undefined, a)+1)],

    constants(R3,[ a,=,1,+,1,.,      % redefined constant
                a,=,1,+,2,.],[]),
    R3 = [a=2, error(constant, error(redefined, a)=3)],

    constants(R4,[ a,=,0,//,0,.],[]),% zero div
    R4 = [error(expression, a=0//0)].