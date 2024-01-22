% w/o accumulator
% ?- profile(len([1,2,3,4,5,6,7,8],N))
% N = 8
% (8 calls in total 1 exit)
len([],0).
len([_|T],N) :- len(T,X), N is X+1.


% w/ accumulator
% bc tail recursion optimization
% ?- profile(accLen([1,2,3,4,5,6,7,8],0,N))
% N = 8
% !!! (1 calls in total 1 exit)
accLen([_|T],A,L) :- Anew is A+1, accLen(T,Anew,L).
accLen([],A,A).

% its a fold
% so, how can i make findall/3 into a fold?
