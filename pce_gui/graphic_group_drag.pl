:- use_module(library(pce)).

main:-
    new(W, window('drag')),
    send(W, size, size(400, 400)),
    send(W, open),
    
    % create Device (group)
    new(Device, device),
    
    % create circles
    new(C1, circle(20)),
    new(C2, circle(20)),
    new(C3, circle(20)),
    
    % add circles to device (group)
    send(Device, display, C1, point(10, 10)),
    send(Device, display, C2, point(50, 10)),
    send(Device, display, C3, point(25, 40)),
    
    % configure drag gesture
    new(DragMove, move_gesture(left)),
    send(Device, recogniser, DragMove),
    
    % display device (group) in window
    send(W, display, Device, point(100, 100)).
