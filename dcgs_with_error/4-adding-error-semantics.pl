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
constants(Bs,Bs) --> [].

constdef(Bs,Id=Val) -->
    const(Bs,Id), [=], expr(Bs,Expr), ['.'],
    { catch( Val is Expr, _, fail ) }.
    % In case of zero divisor or arithmetic overflow,
    % this rule must fail.

expr(Bs,Expr), ['.'] -->
    operand(Bs,X), operator(Op), operand(Bs,Y), ['.'],
    { Expr =.. [Op,X,Y] }. % weird operator here  Exp =.. [+,1,2]. %Exp = 1+2. wowo
    % X and Y are the values of the operands.

operator(Op) --> [Op], { op(Op) }.
op(+). op(-). op(*). op(//). op(mod).

operand(_,X) --> [Y], { integer(Y), !, X = Y }.
operand(Bs,X) --> id(Y), { member(Y=Z,Bs) -> X = Z }.

const(Bs,Id) --> id(Id), { \+member(Id=_,Bs) }.

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

  \+constants(_,[ a,=,a,+,1,.],[]), % depends on undefined constant
  \+constants(_,[ a,=,1,+,1,.,      % redefined constant
                a,=,1,+,2,.],[]),
  \+constants(_,[ a,=,0,//,0,.],[]).% zero div