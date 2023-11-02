#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <X11/Xlib.h>
#include "miniglut.h"
#include "font.h"
#include "term.h"

static int init(void);
static void cleanup(void);

static void display(void);
static void reshape(int x, int y);
static void keypress(unsigned char key, int x, int y);
static void keyrelease(unsigned char key, int x, int y);
static void sighandler(int s);

int win_width, win_height;
int pty, pid;


int main(int argc, char **argv)
{
	int xfd, maxfd, rdsz;
	char *shell;
	fd_set rdset;
	char buf[1024];
	struct timeval tv;

	if(!(shell = getenv("SHELL"))) {
		shell = "/bin/sh";
	}
	if((pty = open("/dev/ptmx", O_RDWR)) == -1) {
		fprintf(stderr, "failed to open pseudoterminal\n");
		return 1;
	}
	grantpt(pty);

	if(!(pid = fork())) {
		setsid();
		unlockpt(pty);

		close(0);
		close(1);
		close(2);
		if(open(ptsname(pty), O_RDWR) == -1) {
			_exit(1);
		}
		close(pty);
		dup(0);
		dup(0);

		putenv("TERM=adm3a");

		execl(shell, shell, (void*)0);
		_exit(1);
	}

	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
	glutInitWindowSize(800, 600);
	glutCreateWindow("termu");

	glutDisplayFunc(display);
	glutReshapeFunc(reshape);
	glutKeyboardFunc(keypress);
	glutKeyboardUpFunc(keyrelease);

	if(init() == -1) {
		kill(pid, SIGINT);
		return 1;
	}
	atexit(cleanup);

	signal(SIGCHLD, sighandler);

	xfd = miniglut_x11_socket();
	maxfd = xfd > pty ? xfd : pty;

	for(;;) {
		FD_ZERO(&rdset);
		FD_SET(pty, &rdset);
		FD_SET(xfd, &rdset);

		tv.tv_sec = 0;
		tv.tv_usec = 250000;

		XFlush(miniglut_x11_display());
		while(select(maxfd + 1, &rdset, 0, 0, &tv) == -1 && errno == EINTR);

		glutPostRedisplay();

		if(FD_ISSET(pty, &rdset)) {
			rdsz = read(pty, buf, sizeof buf);
			if(term_proc(buf, rdsz)) {
				glutPostRedisplay();
			}
		}
		/*if(FD_ISSET(xfd, &rdset)) {*/
			glutMainLoopEvent();
		/*}*/
	}

	return 0;
}

static int init(void)
{
	int width, height;

	text_init();
	term_init();

	width = TERM_COLS * fontw * 2;
	height = width / 1.333333333;
	glutReshapeWindow(width, height);
	glutPostRedisplay();

	return 0;
}

static void cleanup(void)
{
	text_cleanup();
}

static void display(void)
{
	int i, j;
	unsigned char *scrptr = scrbuf;
	unsigned int msec = glutGet(GLUT_ELAPSED_TIME);

	glClearColor(0.1, 0.1, 0.1, 1);
	glClear(GL_COLOR_BUFFER_BIT);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, TERM_COLS, 0, TERM_ROWS, -1, 1);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	text_begin();
	for(i=0; i<TERM_ROWS; i++) {
		for(j=0; j<TERM_COLS; j++) {
			if(*scrptr && *scrptr != ' ') {
				draw_glyph(j, i, *scrptr);
			}
			scrptr++;
		}
	}
	text_end();

	if(cursor_vis && (!cursor_blink || ((msec >> 9) & 1))) {
		int y = 23 - cursor_y;

		glLogicOp(GL_XOR);
		glEnable(GL_COLOR_LOGIC_OP);

		glColor3f(1, 1, 1);
		glRectf(cursor_x + 0.1f, y + 1.0 / 9.0, cursor_x + 1, y + 0.95f);

		glDisable(GL_COLOR_LOGIC_OP);
	}

	glutSwapBuffers();
}

static void reshape(int x, int y)
{
	win_width = x;
	win_height = y;

	glViewport(0, 0, x, y);
}

static unsigned char shifted[] = {
	"\000\001\002\003\004\005\006\007\010\011\012\013\014\015\016\017"
	"\020\021\022\023\024\025\026\027\030\031\032\033\034\035\036\037"
	"\040\041\042\043\044\045\046\"\050\051\052\053<_>?"
	")!@#$%^&*(;:<+>?"
	"@ABCDEFGHIJKLMNO"
	"PQRSTUVWXYZ{|}^_"
	"~ABCDEFGHIJKLMNO"
	"PQRSTUVWXYZ{|}~\077"
};

static void keypress(unsigned char key, int x, int y)
{
	unsigned int mod = glutGetModifiers();

	switch(key) {
	case 'q':
		if(mod & GLUT_ACTIVE_CTRL) {
			exit(0);
		}
		break;

	default:
		break;
	}

	if(mod & GLUT_ACTIVE_SHIFT) {
		if(key < 0x80) {
			key = shifted[key];
		}
	}

	write(pty, &key, 1);
}

static void keyrelease(unsigned char key, int x, int y)
{
}

static void sighandler(int s)
{
	switch(s) {
	case SIGCHLD:
		printf("shell exited, closing terminal\n");
		wait(0);
		exit(0);
	}
}
