
% strip_line_comment(+Line, -Stripped): drop a trailing "% ..." comment,
% respecting double- and single-quoted regions (mirrors src/parse.c).
strip_line_comment(Line, Stripped) :-
    atom_codes(Line, Cs),
    slc(Cs, out, Out),
    atom_codes(Stripped, Out).

slc([], _, []) :- !.
slc([0'\\, E|Cs], dq, [0'\\, E|Out]) :- !, slc(Cs, dq, Out).
slc([0'\\, E|Cs], sq, [0'\\, E|Out]) :- !, slc(Cs, sq, Out).
slc([0'\', 0'\'|Cs], sq, [0'\', 0'\'|Out]) :- !, slc(Cs, sq, Out).
slc([0'"|Cs], dq, [0'"|Out]) :- !, slc(Cs, out, Out).
slc([0'"|Cs], out, [0'"|Out]) :- !, slc(Cs, dq, Out).
slc([0'\'|Cs], sq, [0'\'|Out]) :- !, slc(Cs, out, Out).
slc([0'\'|Cs], out, [0'\'|Out]) :- !, slc(Cs, sq, Out).
slc([0'%|_], out, []) :- !.
slc([C|Cs], State, [C|Out]) :- slc(Cs, State, Out).

% has_complete_clause(+Buf): true if Buf (an atom) contains a terminating
% '.' at bracket depth 0, outside any quoted region, and not part of a
% multi-dot operator token like "=..". mirrors src/parse.c.
has_complete_clause(Buf) :-
    atom_codes(Buf, Cs),
    hcc(Cs, out, 0, 32).

hcc([], _, _, _) :- fail.
hcc([0'\\, _|Cs], dq, D, _) :- !, hcc(Cs, dq, D, 0'\\).
hcc([0'\\, _|Cs], sq, D, _) :- !, hcc(Cs, sq, D, 0'\\).
hcc([0'\', 0'\'|Cs], sq, D, _) :- !, hcc(Cs, sq, D, 0'\').
hcc([0'"|Cs], dq, D, _) :- !, hcc(Cs, out, D, 0'").
hcc([0'"|Cs], out, D, _) :- !, hcc(Cs, dq, D, 0'").
hcc([0'\'|Cs], sq, D, _) :- !, hcc(Cs, out, D, 0'\').
hcc([0'\'|Cs], out, D, _) :- !, hcc(Cs, sq, D, 0'\').
hcc([C|Cs], out, D, _) :- ( C =:= 0'( ; C =:= 0'[ ), !, D1 is D + 1, hcc(Cs, out, D1, C).
hcc([C|Cs], out, D, _) :- ( C =:= 0') ; C =:= 0'] ), !, D1 is D - 1, hcc(Cs, out, D1, C).
hcc([0'.|Cs], out, 0, Prev) :-
    !,
    ( Prev =:= 0'.
    -> hcc(Cs, out, 0, 0'.)
    ;  ( Cs == []
       -> true
       ;  Cs = [N|_], ws_code(N)
       -> true
       ;  hcc(Cs, out, 0, 0'.)
       )
    ).
hcc([C|Cs], State, D, _) :- hcc(Cs, State, D, C).

ws_code(32).  % space
ws_code(0'\t).
ws_code(0'\n).
ws_code(0'\r).

trim_leading(Atom, Trimmed) :-
    atom_codes(Atom, Cs),
    tl_codes(Cs, Cs2),
    atom_codes(Trimmed, Cs2).

tl_codes([C|Cs], Out) :- (C =:= 32 ; C =:= 0'\t), !, tl_codes(Cs, Out).
tl_codes(Cs, Cs).

% consult_dcg(+File): like consult/1, but --> clauses are run through
% dcg_translate/2 before being asserted.
consult_dcg(File) :-
    open(File, read, S),
    dcg_load_loop(S, ''),
    close(S).

dcg_load_loop(S, Buf0) :-
    read_line_to_atom(S, Line0),
    ( Line0 == end_of_file
    -> true
    ;  strip_line_comment(Line0, Line),
       dcg_accumulate(Buf0, Line, Buf1),
       ( Buf1 \== '', has_complete_clause(Buf1)
       -> dcg_load_one(Buf1),
          dcg_load_loop(S, '')
       ;  dcg_load_loop(S, Buf1)
       )
    ).

dcg_accumulate(Buf0, Line, Buf) :-
    trim_leading(Line, Trimmed),
    ( Buf0 == '', Trimmed == ''
    -> Buf = Buf0
    ;  Buf0 == ''
    -> Buf = Trimmed
    ;  atom_concat(Buf0, ' ', Buf1), atom_concat(Buf1, Trimmed, Buf)
    ).

dcg_load_one(Buf) :-
    atom_to_term(Buf, Term, _),
    (  Term = (_ --> _)
    -> dcg_translate(Term, Term2), assertz(Term2)
    ;  Term = (:- Goal)
    -> catch(call(Goal), _, true)
    ;  assertz(Term)
    ).
