#define GL_GLEXT_PROTOTYPES

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <X11/Xlib.h>
#include "miniglut.h"
#include <GL/glu.h>
#include <imago2.h>
#include "font.h"
#include "term.h"

#define GLOW_PASSES		40
#define GLOW_SPREAD		3.5f
#define GLOW_STR		4.0f

#define FBO_WIDTH	800
#define FBO_HEIGHT	600

#define CRT_URES	64
#define CRT_VRES	64

struct image_data {
	char *start;
	long size, cur;
};

static int init(void);
static void cleanup(void);

static void display(void);
static void reshape(int x, int y);
static void keypress(unsigned char key, int x, int y);
static void keyrelease(unsigned char key, int x, int y);
static void sighandler(int s);

static size_t imgread(void *buf, size_t bytes, void *uptr);
static long imgseek(long offs, int whence, void *uptr);

static int win_width, win_height;
static int pty, pid;
static int tubemesh;
static unsigned int fbo, fbtex;
static unsigned int bezeltex;

static float glowtab[GLOW_PASSES][3];

extern char bezel_imgdata[];
extern char bezel_imgdata_end[];

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
	glutInitWindowSize(1440, 1080);
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
	int i, j;
	float u, v, du, dv;
	struct img_io imgio;
	struct image_data imgdata;

	text_init();
	term_init();

	glGenTextures(1, &fbtex);
	glBindTexture(GL_TEXTURE_2D, fbtex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, FBO_WIDTH, FBO_HEIGHT, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, 0);

	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbtex, 0);

	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		fprintf(stderr, "incomplete framebuffer object\n");
		return 1;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	du = 1.0f / CRT_URES;
	dv = 1.0f / CRT_VRES;

#define VERTEX(u, v) \
	do { \
		float x = (u) - 0.5f; \
		float y = (v) - 0.5f; \
		float d = sqrt(x * x + y * y); \
		float z = cos(d * M_PI) * 0.1; \
		glTexCoord2f(u, v); \
		glVertex3f(x, y, z); \
	} while(0)

	tubemesh = glGenLists(1);
	glNewList(tubemesh, GL_COMPILE);
	glBegin(GL_QUADS);
	v = 0.0f;
	for(i=0; i<CRT_VRES; i++) {
		u = 0.0f;
		for(j=0; j<CRT_URES; j++) {
			VERTEX(u, v);
			VERTEX(u + du, v);
			VERTEX(u + du, v + dv);
			VERTEX(u, v + dv);
			u += du;
		}
		v += dv;
	}
	glEnd();
	glEndList();

	imgdata.start = bezel_imgdata;
	imgdata.size = bezel_imgdata_end - bezel_imgdata;
	imgdata.cur = 0;
	imgio.uptr = &imgdata;
	imgio.read = imgread;
	imgio.seek = imgseek;
	bezeltex = img_gltexture_read(&imgio);

	for(i=0; i<GLOW_PASSES; i++) {
		float dx = (float)rand() / (float)RAND_MAX - 0.5f;
		float dy = (float)rand() / (float)RAND_MAX - 0.5f;
		float alpha = sqrt(dx * dx + dy * dy);
		glowtab[i][0] = dx * GLOW_SPREAD;
		glowtab[i][1] = dy * GLOW_SPREAD;
		glowtab[i][2] = alpha * GLOW_STR;
	}

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

	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glViewport(0, 0, FBO_WIDTH, FBO_HEIGHT);

	glColor3f(1, 1, 1);
	glClearColor(0, 0, 0, 1);
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

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, win_width, win_height);
	glClearColor(0.4, 0.1, 0.1, 1);


	glClear(GL_COLOR_BUFFER_BIT);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, bezeltex);

	glBegin(GL_TRIANGLES);
	glTexCoord2f(0, 1); glVertex2f(-1, -1);
	glTexCoord2f(2, 1); glVertex2f(3, -1);
	glTexCoord2f(0, -1); glVertex2f(-1, 3);
	glEnd();

	glMatrixMode(GL_PROJECTION);
	gluPerspective(45, (float)win_width / (float)win_height, 0.5, 500.0);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslatef(0, 0, -1.65);
	glScalef(1.4f, 1.0f, 1.0f);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);

	glEnable(GL_CULL_FACE);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, fbtex);

	for(i=0; i<GLOW_PASSES; i++) {
		glPushMatrix();
		glTranslatef(glowtab[i][0] / win_width, glowtab[i][1] / win_height, 0);
		glColor4f(0.5f, 0.7f, 1.0f, glowtab[i][2] / GLOW_PASSES);
		glCallList(tubemesh);
		glPopMatrix();
	}

	glDisable(GL_TEXTURE_2D);
	glDisable(GL_CULL_FACE);

	glDisable(GL_BLEND);

	glutSwapBuffers();
	assert(glGetError() == GL_NO_ERROR);
}

static void reshape(int x, int y)
{
	win_width = x;
	win_height = y;
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

	if(mod & GLUT_ACTIVE_CTRL) {
		key &= 0x1f;
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

static size_t imgread(void *buf, size_t bytes, void *uptr)
{
	struct image_data *imgdata = (struct image_data*)uptr;

	if(imgdata->cur + bytes > imgdata->size) {
		bytes = imgdata->size - imgdata->cur;
	}
	memcpy(buf, imgdata->start + imgdata->cur, bytes);
	imgdata->cur += bytes;
	return bytes;
}

static long imgseek(long offs, int whence, void *uptr)
{
	struct image_data *imgdata = (struct image_data*)uptr;

	switch(whence) {
	case SEEK_SET:
		if(offs < 0) {
			imgdata->cur = 0;
		} else if(offs > imgdata->size) {
			imgdata->cur = imgdata->size;
		} else {
			imgdata->cur = offs;
		}
		break;
	case SEEK_CUR:
		if(offs < -imgdata->cur) {
			imgdata->cur = 0;
		} else if(offs > imgdata->size - imgdata->cur) {
			imgdata->cur = imgdata->size;
		} else {
			imgdata->cur += offs;
		}
		break;
	case SEEK_END:
		if(offs > 0) {
			imgdata->cur = imgdata->size;
		} else if(offs < -imgdata->size) {
			imgdata->cur = 0;
		} else {
			imgdata->cur = imgdata->size + offs;
		}
		break;
	}

	return imgdata->cur;
}
