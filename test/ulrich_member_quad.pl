% ISO Prolog Prologue member/2 Tests
% Ported from https://git.liminal.cafe/byakuren/flowlog.git
% tests/ulrich/member_quad.pl, itself based on
% https://www.complang.tuwien.ac.at/ulrich/iso-prolog/prologue#member

?- member(X, [1, 2]).
   X = 1
;  X = 2.

?- member(1, L).
   L = [1|_A]
;  L = [_A, 1|_B]
;  L = [_A, _B, 1|_C]
;  ..., ad_infinitum.

?- member(X, [y, z|nonlist]).
   X = y
;  X = z.

?- member(X, nonlist).
   false.
