?- include("core.pl").

?- findall(X, member(X, [1,2,3]), All).
?- stats.
?- findall(X, perm([1,2,3], X), All).
?- stats.
?- findall(X, perm([1,2,3,4], X), All).
?- stats.
?- findall(X, perm([1,2,3,4,5], X), All).
?- stats.