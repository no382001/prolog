% core.pl

% --- true/false ---

?- true.
   true.

?- fail.
   false.

% --- unification ---

?- X = hello.
   X = hello.

?- X = 1, Y = 2.
   X = 1, Y = 2.

?- foo = foo.
   true.

?- foo = bar.
   false.

% --- member/2 ---

?- member(X, [a, b, c]).
   X = a
;  X = b
;  X = c.

?- member(d, [a, b, c]).
   false.

% --- append/3 ---

?- append([1, 2], [3, 4], X).
   X = [1, 2, 3, 4].

?- append([], [a], X).
   X = [a].

% --- length/2 ---

?- length([a, b, c], N).
   N = 3.

?- length([], N).
   N = 0.

% --- reverse/2 ---

?- reverse([1, 2, 3], X).
   X = [3, 2, 1].

?- reverse([], X).
   X = [].

% --- between/3 ---

?- between(1, 3, X).
   X = 1
;  X = 2
;  X = 3.

?- between(5, 5, X).
   X = 5.

% --- last/2 ---

?- last(X, [a, b, c]).
   X = c.

% --- arithmetic ---

?- X is 2 + 3.
   X = 5.

?- X is 10 - 4.
   X = 6.

?- X is 3 * 4.
   X = 12.

?- X is 10 / 2.
   X = 5.

?- X is 7 mod 3.
   X = 1.

% --- comparison ---

?- 3 < 5.
   true.

?- 5 < 3.
   false.

?- 3 =< 3.
   true.

?- 3 >= 3.
   true.

?- 3 =:= 3.
   true.

?- 3 =\= 4.
   true.

% --- lists ---

?- X = [1, 2, 3], member(2, X).
   X = [1, 2, 3].

?- append(X, Y, [1, 2]).
   X = [], Y = [1, 2]
;  X = [1], Y = [2]
;  X = [1, 2], Y = [].

% --- with helper clause ---

color(red).
color(green).
color(blue).

?- color(X).
   X = red
;  X = green
;  X = blue.

?- color(yellow).
   false.

% --- findall/3 ---

?- findall(X, member(X, [a, b, c]), L).
   L = [a, b, c].

?- findall(X, member(X, []), L).
   L = [].

% --- once/1 ---

?- once(member(X, [a, b, c])).
   X = a.

% --- negation ---

?- \+ fail.
   true.

?- \+ true.
   false.

% --- disjunction ;/2 ---

?- (true ; true).
   true
;  true.

?- (fail ; true).
   true.

?- (true ; fail).
   true.

?- (fail ; fail).
   false.

?- (X = a ; X = b).
   X = a
;  X = b.

?- findall(X, (X = a ; X = b), L).
   L = [a, b].

?- findall(X, (X = a ; X = b ; X = c), L).
   L = [a, b, c].

?- (fail ; X = hello).
   X = hello.

% --- if-then -> ---

?- (true -> X = yes).
   X = yes.

?- (fail -> X = yes).
   false.

% --- if-then-else (-> ;) ---

?- (true -> X = yes ; X = no).
   X = yes.

?- (fail -> X = yes ; X = no).
   X = no.

?- (true -> true ; fail).
   true.

?- (fail -> true ; true).
   true.

?- (fail -> true ; fail).
   false.

% -> commits: only first solution of condition
?- findall(X, ((X = a ; X = b) -> true ; true), L).
   L = [a].

% nested if-then-else
?- (true -> (true -> X = deep ; X = no) ; X = outer).
   X = deep.

?- (fail -> X = yes ; (true -> X = inner ; X = no)).
   X = inner.

% if-then-else with arithmetic
?- (1 =:= 1 -> X = equal ; X = not_equal).
   X = equal.

?- (1 =:= 2 -> X = equal ; X = not_equal).
   X = not_equal.
