:- use_module(library(pio)).
:- use_module(library(dcg/basics)).

% - - - - - - - - - - - - - - - - - - - - - - - - - - - -
% --> translated to swi from sictus
% https://web.archive.org/web/20230103115618/http://aszt.inf.elte.hu/~asvanyi/pp/dcgs1p3.pdf
%   DCGS FOR PARSING AND ERROR HANDLING : TIBOR ASVANYI
%
% 3.2. Adding context-dependent, syntactical information.
% - - - - - - - - - - - - - - - - - - - - - - - - - - - -

% Bs is the list of constant identifiers defined up till now.
% C is the identifier of the first constant described
% by this very rule:
constants --> constants([]).
constants(Bs) --> constdef(Bs,C), !, constants([C|Bs]).
constants(_) --> [].

constdef(Bs,Id) --> const(Bs,Id), [=], expr(Bs), ['.'].

expr(Bs), ['.']--> operand(Bs), operator, operand(Bs), ['.'].

operator --> [Op], { op(Op) }.

op(+).
op(-).
op(*).
op(//).
op(mod).

operand(_) --> [Y], { integer(Y), ! }.
% If an operand of an expression is an id,
% it must have been defined before:
operand(Bs) --> id(Y), { member(Y,Bs) -> true }.
% The constant should not have been defined before:
const(Bs,Id) --> id(Id), { \+member(Id,Bs) }.

id(Id) --> [Id], {is_id(Id)}.


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
    constants([ a,=,1,+,1,.,
                aa,=,a,-,1,.,
                b,=,3,*,a,.,
                cc,=,b,//,a,.,
                d,=,b,mod,4,.],[]),
\+ constants([ a,=,a,+,1,.],[]), % depends on undefined constant
\+ constants([ a,=,1,+,1,.,      % redefined constant
               a,=,1,+,2,.],[]).