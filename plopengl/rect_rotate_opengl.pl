:- use_foreign_library("../lib/plOpenGL.so").
:- include("../prolog/plGL_defs.pl").
:- include("../prolog/plGLU_defs.pl").
:- include("../prolog/plGLUT_defs.pl").
:- use_module("../prolog/plOpenGL").
:- use_module("../prolog/plGL").
:- use_module("../prolog/plGLU").
:- use_module("../prolog/plGLUT").

:- dynamic camera_rotation/1.
camera_rotation(0.0).

width(500).
height(500).

camera :-
	camera_rotation(Angle),
    NewAngle is Angle + 0.01,
    retract(camera_rotation(_)),
    assert(camera_rotation(NewAngle)),

    Radius = 10.0,
    EyeX is cos(NewAngle) * Radius,
    EyeZ is sin(NewAngle) * Radius,
	UpX is cos(NewAngle),
    UpY is sin(NewAngle),

	gluLookAt(EyeX,0.0,EyeZ,0.0, 0.0, 0.0, UpX, UpY, 0.0).

display:-
    % defs
    kGL_COLOR_BUFFER_BIT(COLOR_BUFFER),
	kGL_DEPTH_BUFFER_BIT(DEPTH_BUFFER),
	% gl code
	glClear(COLOR_BUFFER \/ DEPTH_BUFFER),
	glColor3f(1.0, 1.0, 1.0),
	glLoadIdentity,

	camera,

	glScalef(1.0, 2.0, 1.0),
	glutWireCube(1.0),
	glFlush,
	sleep(10),
	glutSwapBuffers.

init:-
	% defs
	kGL_FLAT(FLAT),
	% gl code
	glClearColor(0.0, 0.0, 0.0, 0.0),
	glShadeModel(FLAT).


reshape:-
	% defs
	X is 0,
	Y is 0,
	width(W),
	width(H),
	kGL_PROJECTION(PROJECTION),
	kGL_MODELVIEW(MODELVIEW),
	% gl code
	glViewport(X,Y,W,H),
	glMatrixMode(PROJECTION),
	glLoadIdentity,
	glFrustum(-1.0, 1.0, -1.0, 1.0, 1.5, 20.0),
	glMatrixMode(MODELVIEW).


% 27 is ASCII Code for Escape
keyboard(27,_,_) :-
	write('Closing Window and Exiting...'),nl,
	glutDestroyWindow.

idle :- display.

main:-
	% defs
	width(W),
	height(H),
	kGLUT_SINGLE(SINGLE),
	kGLUT_RGB(RGB),
	% gl code
	glutInit,
	glutInitDisplayMode(SINGLE \/ RGB),
	glutInitWindowSize(W, H),
	glutInitWindowPosition(0,0),
	glutCreateWindow('Rect'),
	init,
	glutDisplayFunc,
	glutIdleFunc(idle),
	glutReshapeFunc,
	glutKeyboardFunc,
	glutMainLoop.
