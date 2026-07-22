?- has_complete_clause('?- number_codes(33.0, [0''3|_L]).').
   true.

?- has_complete_clause('?- X = 0''\\n.').
   true.

?- has_complete_clause('?- X is 0''\\\\ + 1.').
   true.

?- has_complete_clause('?- foo([0''a, 0''b], X).').
   true.

?- has_complete_clause('?- X = 0''a, Y = ''real atom''.').
   true.

?- \+ has_complete_clause('?- X = ''unterminated atom.').
   true.

?- strip_terminating_dot('X is 0''a.', Y).
   Y = 'X is 0\'a'.

?- strip_terminating_dot('X is 0''a', Y).
   Y = 'X is 0\'a'.

?- strip_line_comment('X is 0''a. % a comment', Y).
   Y = 'X is 0\'a. '.
