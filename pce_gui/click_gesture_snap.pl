:- use_module(library(pce)).
:- dynamic(mouse_offset/2).
:- dynamic(mouse_last_pos/2).

% forall
draw_points(_, []).  
draw_points(W, [(X,Y)|T]) :-
    mouse_offset(XOffset,YOffset),
    XNew is X + XOffset,
    YNew is Y + YOffset,
    new(R, box(30, 30)),
    send(W, display, R, point(XNew, YNew)),
    %format("point X: ~w, Y: ~w~n", [XNew, YNew]),
    send(R, fill_pattern, colour(black)),
    draw_points(W, T).
    
event_handler(Ev) :-
    get(Ev, x, X),
    get(Ev, y, Y),
    mouse_last_pos(Lx,Ly),
    %format("end X: ~w, Y: ~w~n", [X, Y]),
    retract(mouse_offset(_,_)),
    assert(mouse_offset(X,Y)).

game_loop(W, Points) :-
    send(W, clear),
    draw_points(W, Points),
    sleep(1), % sleeps for 1 second
    game_loop(W, Points).

draw_window(W):-
    new(W, window('drag', size(300, 300))),
    send(W, open),
    new(B, box(300, 300)),
    send(W, display, B, point(0, 0)),
    send(B, fill_pattern, colour(white)).

main :-
    assert(mouse_offset(0,0)),
    assert(mouse_last_pos(0,0)),
    draw_window(W),

    send(W, recogniser, click_gesture(left, '', single, message(@prolog, event_handler, @event))),
    Points = [(0,0),(30, 30), (60, 60), (90, 90)],
    game_loop(W, Points).
