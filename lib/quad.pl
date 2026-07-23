:- consult('dcg_load.pl').

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
% 0'c character-code literal: the lone quote is not a quoted-atom opener.
std_codes([0'0, 0'\', 0'\\, Esc|Cs], out, D, _, [0'0, 0'\', 0'\\, Esc|Out]) :-
    !, std_codes(Cs, out, D, Esc, Out).
std_codes([0'0, 0'\', Ch|Cs], out, D, _, [0'0, 0'\', Ch|Out]) :-
    !, std_codes(Cs, out, D, Ch, Out).
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

parse_expected(AnswerRaw, Expected, Mode) :-
    strip_terminating_dot(AnswerRaw, Answer),
    split_nl(Answer, Lines),
    pe_group(Lines, Groups),
    maplist(pe_join_trim, Groups, Expected0),
    exclude(==(''), Expected0, Expected1),
    ( append(Prefix, ['..., ad_infinitum'], Expected1)
    -> Mode = ad_infinitum, Expected = Prefix
    ;  Mode = exact, Expected = Expected1
    ).

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

% stat/record bookkeeping happens here, right where Pass is freshly bound,
% not back in the caller after the once/1 boundary (saw stale bindings there).
run_one_test(QueryRaw, AnswerRaw, Pass) :-
    strip_query_prefix(QueryRaw, Query0),
    strip_terminating_dot(Query0, Query),
    parse_expected(AnswerRaw, Expected, Mode),
    quad_display(Query, Display),
    quad_ckpt_before(Display),
    get_time_ms(T0),
    ( atom_to_term(Query, QueryTerm, NameVars)
    -> ( catch(
             ( collect_solutions(QueryTerm, NameVars, 64, Got), Error = none ),
             Ball,
             ( Got = [], quad_error_type(Ball, Error) )
         )
       -> true
       ;  Got = [], Error = none
       )
    ;  Got = [], Error = quad_unparseable
    ),
    get_time_ms(T1),
    ElapsedMs is T1 - T0,
    quad_judge(Expected, Got, Error, Mode, Pass, Reason),
    retract(quad_stat(TN0, P0, F0, TMs0)),
    TestNum is TN0 + 1,
    ( Pass == true -> P1 is P0 + 1, F1 = F0 ; P1 = P0, F1 is F0 + 1 ),
    TMs1 is TMs0 + ElapsedMs,
    assertz(quad_stat(TestNum, P1, F1, TMs1)),
    assertz(quad_record(TestNum, Display, Pass, Reason, ElapsedMs)),
    quad_ckpt_after(Display, Pass, Reason, ElapsedMs),
    quad_report(TestNum, Display, Pass, Reason, ElapsedMs),
    !.

% JUnit-mode crash checkpointing: when quad_ckpt_ctx/3 is set, each test
% durably records itself before/after running so a crash mid-file is
% recoverable instead of silently dropping the whole suite's report.
:- dynamic(quad_ckpt_ctx/3).

quad_ckpt_before(Display) :-
    ( quad_ckpt_ctx(ProgressPath, _, _)
    -> catch(
           ( open(ProgressPath, write, S),
             write(S, Display), nl(S),
             close(S)
           ), _, true)
    ;  true
    ).

quad_ckpt_after(Display, Pass, Reason, ElapsedMs) :-
    ( quad_ckpt_ctx(_, PartialPath, Suite)
    -> catch(
           ( open(PartialPath, append, S),
             write_testcase(S, Suite, Display, Pass, Reason, ElapsedMs),
             close(S)
           ), _, true)
    ;  true
    ).

% error(Type, _) balls reduce to Type; any other thrown term (e.g. a bare
% atom from a non-matching catcher) is used as-is instead of crashing the run.
quad_error_type(error(Type, _), Type) :- !.
quad_error_type(Ball, Ball).

quad_judge(Expected, Got, Error, Mode, Pass, Reason) :-
    ( Mode == ad_infinitum
    -> quad_judge_ad_infinitum(Expected, Got, Error, Pass, Reason)
    ;  quad_judge_exact(Expected, Got, Error, Pass, Reason)
    ).

% witness sampling for an "ad infinitum" test
quad_ad_infinitum_witness(8).

quad_judge_ad_infinitum(Expected, Got, Error, Pass, Reason) :-
    ( Error \== none
    -> Pass = false, format_atom('error: ~w', [Error], Reason)
    ;  length(Expected, PrefixLen),
       quad_ad_infinitum_witness(Witness),
       MinLen is PrefixLen + Witness,
       length(GotPrefix, PrefixLen),
       append(GotPrefix, _, Got),
       GotPrefix == Expected,
       length(Got, GotLen),
       GotLen >= MinLen
    -> Pass = true, Reason = ''
    ;  quad_ad_infinitum_witness(Witness2),
       format_atom('expected: ~w, then ..., ad_infinitum (at least ~w more)~ngot: ~w',
                   [Expected, Witness2, quad_got(Got,Error)], Reason),
       Pass = false
    ).

quad_judge_exact(Expected, Got, Error, Pass, Reason) :-
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
    atom_chars(Fmt, Cs),
    fw_codes(Cs, Args).

fw_codes([], []).
fw_codes(['~', w|Cs], [A|As]) :- !, write(A), fw_codes(Cs, As).
fw_codes(['~', n|Cs], As) :- !, nl, fw_codes(Cs, As).
fw_codes([C|Cs], As) :- put_chars([C]), fw_codes(Cs, As).

quad_report(TestNum, Display, true, _, ElapsedMs) :-
    !,
    ms_to_secs_atom(ElapsedMs, TimeAtom),
    write('ok '), write(TestNum), write(' - ?- '), write(Display),
    write(' # time='), write(TimeAtom), write('s'), nl.
quad_report(TestNum, Display, false, Reason, ElapsedMs) :-
    ms_to_secs_atom(ElapsedMs, TimeAtom),
    write('not ok '), write(TestNum), write(' - ?- '), write(Display),
    write(' # time='), write(TimeAtom), write('s'), nl,
    write_reason_lines(Reason).

write_reason_lines(Reason) :-
    split_nl(Reason, Lines),
    forall(member(L, Lines), (write('#   '), write(L), nl)).

%****
%* elapsed-time formatting: get_time_ms/1 (registered as an FFI builtin in
%* main.c) gives integer milliseconds; format as "S.mmm" for TAP/JUnit.
%****

pad3(N, Atom) :-
    atom_number(NAtom, N),
    ( N < 10 -> atom_concat('00', NAtom, Atom)
    ; N < 100 -> atom_concat('0', NAtom, Atom)
    ; Atom = NAtom
    ).

ms_to_secs_atom(Ms, Atom) :-
    Secs is Ms // 1000,
    Frac is Ms mod 1000,
    pad3(Frac, FracAtom),
    atom_number(SecsAtom, Secs),
    atom_concat(SecsAtom, '.', A1),
    atom_concat(A1, FracAtom, Atom).

%****
%* quad file parsing and running
%****

:- dynamic(quad_stat/4).
:- dynamic(quad_record/5).

run_quad_file(File) :- run_quad_file(File, 0).

% Skip: how many query/answer blocks to re-parse but not re-execute
% (directives/facts still run either way).
run_quad_file(File, Skip) :-
    retractall(quad_stat(_, _, _, _)),
    assertz(quad_stat(0, 0, 0, 0)),
    retractall(quad_record(_, _, _, _, _)),
    open(File, read, S),
    qf_loop(S, '', '', query, Skip, 0),
    close(S),
    quad_stat(Total, Passed, Failed, TotalMs),
    ms_to_secs_atom(TotalMs, TotalTimeAtom),
    write('# '), write(File), write(': '), write(Total), write(' tests, '),
    write(Passed), write(' passed, '), write(Failed), write(' failed, '),
    write(TotalTimeAtom), write('s total'), nl.

qf_loop(S, ClauseBuf, AnswerBuf, Mode, Skip, SeenCount) :-
    read_line_to_atom(S, Line0),
    ( Line0 == end_of_file
    -> true
    ;  strip_line_comment(Line0, Line),
       trim_leading(Line, Trimmed),
       ( Mode == answer
       -> qf_answer_step(S, ClauseBuf, AnswerBuf, Line, Trimmed, Skip, SeenCount)
       ;  qf_clause_step(S, ClauseBuf, Line, Trimmed, Skip, SeenCount)
       )
    ).

qf_answer_step(S, QueryBuf, AnswerBuf0, Line, Trimmed, Skip, SeenCount) :-
    ( AnswerBuf0 == '', Trimmed == ''
    -> qf_loop(S, QueryBuf, AnswerBuf0, answer, Skip, SeenCount)
    ;  ( AnswerBuf0 == '' -> AnswerBuf1 = Line
       ;  atom_concat(AnswerBuf0, '\n', AB1), atom_concat(AB1, Line, AnswerBuf1)
       ),
       ( has_complete_clause(AnswerBuf1)
       -> SeenCount1 is SeenCount + 1,
          ( SeenCount1 =< Skip
          -> true % already resolved by an earlier attempt; don't re-run it
          ;  once(run_one_test(QueryBuf, AnswerBuf1, _Pass))
          ),
          qf_loop(S, '', '', query, Skip, SeenCount1)
       ;  qf_loop(S, QueryBuf, AnswerBuf1, answer, Skip, SeenCount)
       )
    ).

qf_clause_step(S, ClauseBuf0, Line, Trimmed, Skip, SeenCount) :-
    dcg_accumulate(ClauseBuf0, Trimmed, ClauseBuf1),
    ( has_complete_clause(ClauseBuf1)
    -> ( sub_atom(ClauseBuf1, 0, 2, _, '?-')
       -> qf_loop(S, ClauseBuf1, '', answer, Skip, SeenCount)
       ;  sub_atom(ClauseBuf1, 0, 2, _, ':-')
       -> sub_atom(ClauseBuf1, 2, _, 0, DirText0),
          strip_terminating_dot(DirText0, DirText),
          catch((atom_to_term(DirText, Goal, _),
                 with_output_to(atom(_), catch(call(Goal), _, true))), _, true),
          qf_loop(S, '', '', query, Skip, SeenCount)
       ;  strip_terminating_dot(ClauseBuf1, ClauseText),
          catch((atom_to_term(ClauseText, Term, _), assertz(Term)), _, true),
          qf_loop(S, '', '', query, Skip, SeenCount)
       )
    ;  qf_loop(S, ClauseBuf1, '', query, Skip, SeenCount)
    ).

%****
%* junit xml output
%****

% text after the final '/' (or all of Path): the first left-to-right split
% whose suffix has no further '/' is necessarily the last one.
last_path_segment(Path, Seg) :-
    atom_codes(Path, Cs),
    ( append(_, [0'/|SegCs], Cs), \+ member(0'/, SegCs)
    -> true
    ;  SegCs = Cs
    ),
    atom_codes(Seg, SegCs).

% strip_ext(+Atom, -Base): text before the final '.', same last-match logic.
strip_ext(Atom, Base) :-
    atom_codes(Atom, Cs),
    ( append(BaseCs, [0'.|Rest], Cs), \+ member(0'., Rest)
    -> true
    ;  BaseCs = Cs
    ),
    atom_codes(Base, BaseCs).

quad_suite_name(File, Suite) :-
    last_path_segment(File, Seg),
    strip_ext(Seg, Suite).

xml_escape(Atom, Escaped) :-
    atom_codes(Atom, Cs),
    xesc_codes(Cs, Out),
    atom_codes(Escaped, Out).

xesc_codes([], []).
xesc_codes([0'&|Cs], [0'&, 0'a, 0'm, 0'p, 0';|Out]) :- !, xesc_codes(Cs, Out).
xesc_codes([0'<|Cs], [0'&, 0'l, 0't, 0';|Out]) :- !, xesc_codes(Cs, Out).
xesc_codes([0'>|Cs], [0'&, 0'g, 0't, 0';|Out]) :- !, xesc_codes(Cs, Out).
xesc_codes([0'"|Cs], [0'&, 0'q, 0'u, 0'o, 0't, 0';|Out]) :-
    !, xesc_codes(Cs, Out).
xesc_codes([0'\n|Cs], [0'&, 0'#, 0'1, 0'0, 0';|Out]) :- !, xesc_codes(Cs, Out).
xesc_codes([0'\r|Cs], [0'&, 0'#, 0'1, 0'3, 0';|Out]) :- !, xesc_codes(Cs, Out).
xesc_codes([C|Cs], [C|Out]) :- xesc_codes(Cs, Out).

write_testcase(Strm, Suite, Name, true, _, ElapsedMs) :-
    !,
    xml_escape(Name, EscName),
    ms_to_secs_atom(ElapsedMs, TimeAtom),
    format_atom('  <testcase name="~w" classname="~w" time="~w"/>',
                [EscName, Suite, TimeAtom], Line),
    write(Strm, Line), nl(Strm).
write_testcase(Strm, Suite, Name, false, Reason, ElapsedMs) :-
    xml_escape(Name, EscName),
    xml_escape(Reason, EscReason),
    ms_to_secs_atom(ElapsedMs, TimeAtom),
    format_atom('  <testcase name="~w" classname="~w" time="~w">',
                [EscName, Suite, TimeAtom], Open),
    write(Strm, Open), nl(Strm),
    format_atom('    <failure message="~w"/>', [EscReason], FailLine),
    write(Strm, FailLine), nl(Strm),
    write(Strm, '  </testcase>'), nl(Strm).

run_quad_file_junit(File, Dir) :- run_quad_file_junit(File, Dir, 0).

% Skip > 0: resuming after a crash, so keep the existing scratch files
% instead of truncating them.
run_quad_file_junit(File, Dir, Skip) :-
    quad_suite_name(File, Suite),
    atom_concat(Dir, '/', D1),
    atom_concat(D1, Suite, D2),
    atom_concat(D2, '.progress', ProgressPath),
    atom_concat(D2, '.xml.partial', PartialPath),
    ( Skip =:= 0
    -> catch((open(ProgressPath, write, S1), close(S1)), _, true),
       catch((open(PartialPath, write, S2), close(S2)), _, true)
    ;  true
    ),
    retractall(quad_ckpt_ctx(_, _, _)),
    assertz(quad_ckpt_ctx(ProgressPath, PartialPath, Suite)),
    run_quad_file(File, Skip),
    retractall(quad_ckpt_ctx(_, _, _)),
    quad_finalize_junit(File, Suite, Dir).

read_whole_file(Path, Whole) :-
    open(Path, read, S),
    rwf_loop(S, '', Whole),
    close(S).

rwf_loop(S, Acc, Whole) :-
    read_line_to_atom(S, Line),
    ( Line == end_of_file
    -> Whole = Acc
    ;  atom_concat(Line, '\n', L1),
       atom_concat(Acc, L1, Acc1),
       rwf_loop(S, Acc1, Whole)
    ).

% counts <testcase>/<failure> lines by prefix while reading, rather than
% scanning the whole body afterward (that hit quad.pl's known LCO
% slowdown on a large partial file).
read_and_count_partial(Path, Body, TestcaseCount, FailureCount) :-
    catch(
        ( open(Path, read, S),
          racp_loop(S, '', 0, 0, Body, TestcaseCount, FailureCount),
          close(S)
        ), _, ( Body = '', TestcaseCount = 0, FailureCount = 0 )).

racp_loop(S, AccBody, AccT, AccF, Body, TotalT, TotalF) :-
    read_line_to_atom(S, Line),
    ( Line == end_of_file
    -> Body = AccBody, TotalT = AccT, TotalF = AccF
    ;  ( line_has_prefix(Line, '  <testcase ') -> AccT1 is AccT + 1 ; AccT1 = AccT ),
       ( line_has_prefix(Line, '    <failure ') -> AccF1 is AccF + 1 ; AccF1 = AccF ),
       atom_concat(Line, '\n', L1),
       atom_concat(AccBody, L1, AccBody1),
       racp_loop(S, AccBody1, AccT1, AccF1, Body, TotalT, TotalF)
    ).

line_has_prefix(Line, Prefix) :-
    atom_length(Prefix, Len),
    atom_length(Line, LineLen),
    LineLen >= Len,
    sub_atom(Line, 0, Len, _, Prefix).

% appends a synthetic "crashed here" <testcase> naming the in-flight
% query, so the next resume attempt's Skip steps past it too.
quad_mark_crash(Suite, Dir) :-
    atom_concat(Dir, '/', D1),
    atom_concat(D1, Suite, D2),
    atom_concat(D2, '.progress', ProgressPath),
    atom_concat(D2, '.xml.partial', PartialPath),
    ( catch(read_whole_file(ProgressPath, CrashedRaw), _, fail)
    -> trim_trailing(CrashedRaw, CrashedDisplay)
    ;  CrashedDisplay = 'unknown query (no checkpoint recorded)'
    ),
    xml_escape(CrashedDisplay, EscCrashed),
    format_atom('  <testcase name="~w (trilog crashed here)" classname="~w" time="0.000">',
                [EscCrashed, Suite], CrashOpen),
    open(PartialPath, append, Strm),
    write(Strm, CrashOpen), nl(Strm),
    write(Strm, '    <failure message="trilog crashed while running this test (see harness log for details)"/>'), nl(Strm),
    write(Strm, '  </testcase>'), nl(Strm),
    close(Strm).

quad_resolved_count(Suite, Dir, Count) :-
    atom_concat(Dir, '/', D1),
    atom_concat(D1, Suite, D2),
    atom_concat(D2, '.xml.partial', PartialPath),
    read_and_count_partial(PartialPath, _Body, Count, _Failed).

quad_finalize_junit(File, Suite, Dir) :-
    atom_concat(Dir, '/', D1),
    atom_concat(D1, Suite, D2),
    atom_concat(D2, '.xml', XmlPath),
    atom_concat(D2, '.progress', ProgressPath),
    atom_concat(D2, '.xml.partial', PartialPath),
    read_and_count_partial(PartialPath, Body, Total, Failed),
    xml_escape(File, EscFile),
    open(XmlPath, write, Strm),
    write(Strm, '<?xml version="1.0" encoding="UTF-8"?>'), nl(Strm),
    format_atom('<testsuite name="~w" file="~w" tests="~w" failures="~w" errors="0" time="0.000">',
                [Suite, EscFile, Total, Failed], Header),
    write(Strm, Header), nl(Strm),
    write(Strm, Body),
    write(Strm, '</testsuite>'), nl(Strm),
    close(Strm),
    catch((open(ProgressPath, write, S1), close(S1)), _, true),
    catch((open(PartialPath, write, S2), close(S2)), _, true).

%****
%* CLI entry points: set the process exit code (0 all pass, 1 any failure)
%****

quad_cli(File) :-
    once(run_quad_file(File)),
    quad_stat(_, _, Failed, _),
    ( Failed > 0 -> halt(1) ; halt(0) ).

quad_cli_junit(File, Dir) :- quad_cli_junit(File, Dir, 0).

quad_cli_junit(File, Dir, Skip) :-
    once(run_quad_file_junit(File, Dir, Skip)),
    quad_stat(_, _, Failed, _),
    ( Failed > 0 -> halt(1) ; halt(0) ).
