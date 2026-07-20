%****
%* line splitting
%****

split_nl(Atom, Lines) :-
    atom_codes(Atom, Cs),
    split_nl_codes(Cs, LinesCodes),
    codes_lists_to_atoms(LinesCodes, Lines).

codes_lists_to_atoms([], []).
codes_lists_to_atoms([Cs|Css], [A|As]) :-
    atom_codes(A, Cs),
    codes_lists_to_atoms(Css, As).

split_nl_codes(Cs, [Line|Rest]) :-
    append(Line, [0'\n|Tail], Cs),
    !,
    split_nl_codes(Tail, Rest).
split_nl_codes(Cs, [Cs]).

trim_trailing(Atom, Trimmed) :-
    atom_codes(Atom, Cs),
    reverse(Cs, R0),
    tt_codes(R0, R1),
    reverse(R1, Cs2),
    atom_codes(Trimmed, Cs2).

tt_codes([C|Cs], Out) :- ws_code(C), !, tt_codes(Cs, Out).
tt_codes(Cs, Cs).

%****
%* answer parsing: "x = a\n;  x = b" -> ['x = a', 'x = b']
%****

% strip_terminating_dot(+Buf, -Stripped): drop the trailing '.' that ends
% the answer block, same rule as has_complete_clause's terminator.
strip_terminating_dot(Buf, Stripped) :-
    atom_codes(Buf, Cs),
    std_codes(Cs, out, 0, 32, Cs2),
    atom_codes(Stripped, Cs2).

std_codes([], _, _, _, []).
std_codes([0'\\, E|Cs], dq, D, _, [0'\\, E|Out]) :-
    !, std_codes(Cs, dq, D, 0'\\, Out).
std_codes([0'\\, E|Cs], sq, D, _, [0'\\, E|Out]) :-
    !, std_codes(Cs, sq, D, 0'\\, Out).
std_codes([0'\', 0'\'|Cs], sq, D, _, [0'\', 0'\'|Out]) :-
    !, std_codes(Cs, sq, D, 0'\', Out).
std_codes([0'"|Cs], dq, D, _, [0'"|Out]) :- !, std_codes(Cs, out, D, 0'", Out).
std_codes([0'"|Cs], out, D, _, [0'"|Out]) :- !, std_codes(Cs, dq, D, 0'", Out).
std_codes([0'\'|Cs], sq, D, _, [0'\'|Out]) :- !, std_codes(Cs, out, D, 0'\', Out).
std_codes([0'\'|Cs], out, D, _, [0'\'|Out]) :- !, std_codes(Cs, sq, D, 0'\', Out).
std_codes([C|Cs], out, D, _, [C|Out]) :-
    (C =:= 0'( ; C =:= 0'[), !, D1 is D + 1, std_codes(Cs, out, D1, C, Out).
std_codes([C|Cs], out, D, _, [C|Out]) :-
    (C =:= 0') ; C =:= 0']), !, D1 is D - 1, std_codes(Cs, out, D1, C, Out).
std_codes([0'.|Cs], out, 0, Prev, Out) :-
    !,
    ( Prev =:= 0'.
    -> Out = [0'.|Out1], std_codes(Cs, out, 0, 0'., Out1)
    ;  ( Cs == []
       -> Out = []
       ;  Cs = [N|_], ws_code(N)
       -> Out = []
       ;  Out = [0'.|Out1], std_codes(Cs, out, 0, 0'., Out1)
       )
    ).
std_codes([C|Cs], State, D, _, [C|Out]) :- std_codes(Cs, State, D, C, Out).

% parse_expected(+AnswerRaw, -Expected): list of expected answer atoms.
parse_expected(AnswerRaw, Expected) :-
    strip_terminating_dot(AnswerRaw, Answer),
    split_nl(Answer, Lines),
    pe_group(Lines, Groups),
    maplist(pe_join_trim, Groups, Expected0),
    exclude(==(''), Expected0, Expected).

% group consecutive lines into one alternative per group, starting a new
% group whenever a line's first non-blank char is ';'.
pe_group([], []).
pe_group([L|Ls], [[L]|Gs]) :- pe_group_(Ls, Gs).

pe_group_([], []).
pe_group_([L|Ls], Gs) :-
    trim_leading(L, T),
    ( T == ''
    -> Gs = Gs1, pe_group_(Ls, Gs1)
    ;  sub_atom(T, 0, 1, _, ';')
    -> sub_atom(T, 1, _, 0, Rest0),
       trim_leading(Rest0, Rest),
       Gs = [[Rest]|Gs1],
       pe_group_(Ls, Gs1)
    ;  Gs = [[L|More]|Gs1],
       pe_group_(Ls, [More|Gs1])
    ).

pe_join_trim(Group, Joined) :-
    pe_join(Group, J0),
    trim_leading(J0, J1),
    trim_trailing(J1, Joined).

pe_join([L], L) :- !.
pe_join([L|Ls], Joined) :-
    pe_join(Ls, Rest),
    atom_concat(L, ' ', L1),
    atom_concat(L1, Rest, Joined).

%****
%* binding formatting: "Name = Value, ..." per solution, matching
%* print_bindings (src/print.c) — skips vars whose name starts with '_',
%* joined by ", ", "true" if nothing to show.
%****

format_bindings(NameVars, Str) :-
    include(shown_binding, NameVars, Shown),
    ( Shown == []
    -> Str = true
    ;  maplist(format_one_binding, Shown, Parts),
       join_atoms(Parts, ', ', Str)
    ).

% print_bindings (src/print.c) only shows variables that actually got
% bound during solving — e.g. findall(R, Goal, L)'s R never touches the
% outer environment — and skips names starting with '_'.
shown_binding(Name = Val) :-
    \+ sub_atom(Name, 0, 1, _, '_'),
    nonvar(Val).

format_one_binding(Name = Val, Part) :-
    term_to_atom(Val, ValAtom),
    atom_concat(Name, ' = ', P0),
    atom_concat(P0, ValAtom, Part).

join_atoms([A], _, A) :- !.
join_atoms([A|As], Sep, Out) :-
    join_atoms(As, Sep, Rest),
    atom_concat(A, Sep, A1),
    atom_concat(A1, Rest, Out).

%****
%* solution collection, capped like MAX_QUAD_ANSWERS in quad.c
%****

:- dynamic(quad_solution_count/1).

collect_solutions(Query, NameVars, Max, Strs) :-
    retractall(quad_solution_count(_)),
    assertz(quad_solution_count(0)),
    with_output_to(atom(_), findall(Str, capped_solution(Query, NameVars, Max, Str), Strs)).

capped_solution(Query, NameVars, Max, Str) :-
    call(Query),
    format_bindings(NameVars, Str),
    retract(quad_solution_count(N)),
    N1 is N + 1,
    assertz(quad_solution_count(N1)),
    ( N1 >= Max -> ! ; true ).

%****
%* running one test
%****

quad_display(Query, Display) :-
    ( atom_length(Query, Len), Len > 60
    -> sub_atom(Query, 0, 57, _, Head), atom_concat(Head, '...', Display)
    ;  Display = Query
    ).

strip_query_prefix(Raw, Query) :-
    ( sub_atom(Raw, 0, 2, _, '?-')
    -> sub_atom(Raw, 2, _, 0, Rest)
    ;  Rest = Raw
    ),
    trim_leading(Rest, Query).

run_one_test(QueryRaw, AnswerRaw, TestNum, Pass) :-
    strip_query_prefix(QueryRaw, Query0),
    strip_terminating_dot(Query0, Query),
    parse_expected(AnswerRaw, Expected),
    quad_display(Query, Display),
    atom_to_term(Query, QueryTerm, NameVars),
    ( catch(
          ( collect_solutions(QueryTerm, NameVars, 64, Got), Error = none ),
          error(ErrType, _),
          ( Got = [], Error = ErrType )
      )
    -> true
    ;  Got = [], Error = none
    ),
    quad_judge(Expected, Got, Error, Pass, Reason),
    quad_report(TestNum, Display, Pass, Reason).

quad_judge(Expected, Got, Error, Pass, Reason) :-
    ( Expected = [false]
    -> ( Got == [], Error == none -> Pass = true, Reason = ''
       ;  Pass = false, format_atom('expected: false~ngot: ~w', [quad_got(Got,Error)], Reason)
       )
    ;  Expected = [ExpErr], is_error_expectation(ExpErr, ExpType)
    -> ( Error \== none, matches_error(Error, ExpType)
       -> Pass = true, Reason = ''
       ;  Pass = false,
          format_atom('expected: ~w~ngot: ~w', [ExpErr, quad_got(Got,Error)], Reason)
       )
    ;  Error \== none
    -> Pass = false, format_atom('error: ~w', [Error], Reason)
    ;  Got == Expected
    -> Pass = true, Reason = ''
    ;  Pass = false, format_atom('expected: ~w~ngot: ~w', [Expected, Got], Reason)
    ).

is_error_expectation(Atom, Type) :-
    atom_concat('error(', Rest, Atom),
    atom_concat(Type, ')', Rest).

matches_error(Error, ExpType) :-
    term_to_atom(Error, ErrAtom),
    ( ErrAtom == ExpType -> true
    ; atom_concat(ExpType, _, ErrAtom) -> true
    ; fail
    ).

format_atom(Fmt, Args, Atom) :- with_output_to(atom(Atom), format_write(Fmt, Args)).

format_write(Fmt, Args) :-
    atom_codes(Fmt, Cs),
    fw_codes(Cs, Args).

fw_codes([], []).
fw_codes([0'~, 0'w|Cs], [A|As]) :- !, write(A), fw_codes(Cs, As).
fw_codes([0'~, 0'n|Cs], As) :- !, nl, fw_codes(Cs, As).
fw_codes([C|Cs], As) :- put_chars([C]), fw_codes(Cs, As).

quad_report(TestNum, Display, true, _) :-
    !,
    write('ok '), write(TestNum), write(' - ?- '), write(Display), nl.
quad_report(TestNum, Display, false, Reason) :-
    write('not ok '), write(TestNum), write(' - ?- '), write(Display), nl,
    write_reason_lines(Reason).

write_reason_lines(Reason) :-
    split_nl(Reason, Lines),
    forall(member(L, Lines), (write('#   '), write(L), nl)).

%****
%* quad file parsing and running
%****

:- dynamic(quad_stat/3).

run_quad_file(File) :-
    retractall(quad_stat(_, _, _)),
    assertz(quad_stat(0, 0, 0)),
    open(File, read, S),
    qf_loop(S, '', '', query),
    close(S),
    quad_stat(Total, Passed, Failed),
    write('# '), write(File), write(': '), write(Total), write(' tests, '),
    write(Passed), write(' passed, '), write(Failed), write(' failed'), nl.

qf_loop(S, ClauseBuf, AnswerBuf, Mode) :-
    read_line_to_atom(S, Line0),
    ( Line0 == end_of_file
    -> true
    ;  strip_line_comment(Line0, Line),
       trim_leading(Line, Trimmed),
       ( Mode == answer
       -> qf_answer_step(S, ClauseBuf, AnswerBuf, Line, Trimmed)
       ;  qf_clause_step(S, ClauseBuf, Line, Trimmed)
       )
    ).

qf_answer_step(S, QueryBuf, AnswerBuf0, Line, Trimmed) :-
    ( AnswerBuf0 == '', Trimmed == ''
    -> qf_loop(S, QueryBuf, AnswerBuf0, answer)
    ;  ( AnswerBuf0 == '' -> AnswerBuf1 = Line
       ;  atom_concat(AnswerBuf0, '\n', AB1), atom_concat(AB1, Line, AnswerBuf1)
       ),
       ( has_complete_clause(AnswerBuf1)
       -> retract(quad_stat(TN0, P0, F0)),
          TN1 is TN0 + 1,
          run_one_test(QueryBuf, AnswerBuf1, TN1, Pass),
          ( Pass == true -> P1 is P0 + 1, F1 = F0 ; P1 = P0, F1 is F0 + 1 ),
          assertz(quad_stat(TN1, P1, F1)),
          qf_loop(S, '', '', query)
       ;  qf_loop(S, QueryBuf, AnswerBuf1, answer)
       )
    ).

qf_clause_step(S, ClauseBuf0, Line, Trimmed) :-
    dcg_accumulate(ClauseBuf0, Trimmed, ClauseBuf1),
    ( has_complete_clause(ClauseBuf1)
    -> ( sub_atom(ClauseBuf1, 0, 2, _, '?-')
       -> qf_loop(S, ClauseBuf1, '', answer)
       ;  sub_atom(ClauseBuf1, 0, 2, _, ':-')
       -> sub_atom(ClauseBuf1, 2, _, 0, DirText0),
          strip_terminating_dot(DirText0, DirText),
          catch((atom_to_term(DirText, Goal, _),
                 with_output_to(atom(_), catch(call(Goal), _, true))), _, true),
          qf_loop(S, '', '', query)
       ;  strip_terminating_dot(ClauseBuf1, ClauseText),
          catch((atom_to_term(ClauseText, Term, _), assertz(Term)), _, true),
          qf_loop(S, '', '', query)
       )
    ;  qf_loop(S, ClauseBuf1, '', query)
    ).
