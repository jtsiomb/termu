#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <imago2.h>
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include "font.h"

/* ADM3a 5x7 glyphs, displayed in a 7x9 matrix */

#define ROM_CHARS	128

#define TEX_NCOLS	16
#define TEX_NROWS	(ROM_CHARS / TEX_NCOLS)

#define CELL_WIDTH	8
#define CELL_HEIGHT	8

#define CELL_SCALE	16
#define SCAN_WIDTH	14

#define TEX_WIDTH	(TEX_NCOLS * CELL_WIDTH * CELL_SCALE)
#define TEX_HEIGHT	(TEX_NROWS * CELL_HEIGHT * CELL_SCALE)

#define UVWIDTH		(1.0f / (float)TEX_NCOLS)
#define UVHEIGHT	(1.0f / (float)TEX_NROWS)

int fontw, fonth;
unsigned int tex_font;
extern unsigned char rom_upper[];
extern unsigned char rom_lower[];

struct vertex {
	float x, y;
	float u, v;
};

#define VB_NGLYPHS	128
#define VB_NVERTS	(VB_NGLYPHS * 4)
static struct vertex varr[VB_NVERTS];
static int nglyphs, cur_vbo;
static unsigned int vbo[2];

static unsigned char cap[CELL_SCALE * CELL_SCALE];

static float smoothstep(float a, float b, float x)
{
	if(x < a) return 0.0f;
	if(x >= b) return 1.0f;

	x = (x - a) / (b - a);
	return x * x * (3.0f - 2.0f * x);
}

static void gen_cap(void)
{
	int i, j;
	unsigned char *dest = cap;

	for(i=0; i<CELL_SCALE; i++) {
		float dy = (float)i - CELL_SCALE * 0.5f;
		for(j=0; j<CELL_SCALE; j++) {
			float dx = (float)j - CELL_SCALE * 0.5f;
			float dist = sqrt(dx * dx + dy * dy);

			float val = 1.0f - smoothstep(SCAN_WIDTH / 2.0f - 1.0f, SCAN_WIDTH / 2.0f + 1.0f, dist);
			*dest++ = (int)(val * 255.0f);
		}
	}

	/*img_save_pixels("cap.png", cap, CELL_SCALE, CELL_SCALE, IMG_FMT_GREY8);*/
}

enum {CAP_START, CAP_MID, CAP_END, EMPTY_START, EMPTY_END};
static int blitcap(unsigned char *dest, int len, int type, int pitch)
{
	int i, j;
	unsigned char *src;

	if(type == EMPTY_START || type == EMPTY_END) {
		src = type == EMPTY_START ? cap : cap + CELL_SCALE / 2;

		for(i=0; i<CELL_SCALE; i++) {
			for(j=0; j<CELL_SCALE/2; j++) {
				dest[j] = (~src[j]);
			}
			dest += pitch;
			src += CELL_SCALE;
		}

		return CELL_SCALE/2;
	}

	if(type == CAP_MID) {
		/* repeat a single columns of pixels */
		for(i=0; i<len; i++) {
			unsigned char *p = dest + i;
			src = cap + CELL_SCALE / 2;
			for(j=0; j<CELL_SCALE; j++) {
				*p = *src;
				p += pitch;
				src += CELL_SCALE;
			}
		}
		return len > 0 ? len : 0;
	}

	src = type == CAP_START ? cap : cap + CELL_SCALE / 2;

	for(i=0; i<CELL_SCALE; i++) {
		for(j=0; j<CELL_SCALE/2; j++) {
			dest[j] = src[j];
		}
		dest += pitch;
		src += CELL_SCALE;
	}
	return CELL_SCALE/2;
}

static int transform(int c)
{
	if(c >= 0x60) {
		return ((~c) & 0x7f) | 0x40;
	}
	return c & 0x3f;
}

void text_init(void)
{
	int i, j, k, len, start, glyph;
	unsigned char *img;
	unsigned char *src, *dest;

	gen_cap();

	dest = img = calloc(TEX_WIDTH, TEX_HEIGHT);

	for(i=0; i<ROM_CHARS; i++) {
		glyph = transform(i + 0x20);
		src = rom_upper + glyph * 8;
		for(j=0; j<8; j++) {
			unsigned char row = (*src++ & 0x1f) << 2;
			start = -1;
			for(k=0; k<8; k++) {
				if((row >> (7 - k)) & 1) {
					if(start == -1) start = k;
				} else {
					if(start >= 0) {
						len = k - start - 1;
						dest += blitcap(dest, 0, CAP_START, TEX_WIDTH);
						if(len > 0) {
							dest += blitcap(dest, CELL_SCALE * len, CAP_MID, TEX_WIDTH);
						}
						dest += blitcap(dest, 0, CAP_END, TEX_WIDTH);
						start = -1;
					}
					dest += CELL_SCALE;
					/*dest += blitcap(dest, 0, EMPTY_START, TEX_WIDTH);
					dest += blitcap(dest, 0, EMPTY_END, TEX_WIDTH);*/
				}
			}
			dest += (TEX_WIDTH - CELL_WIDTH) * CELL_SCALE;
		}

		if((i & 0xf) == 0xf) {
			dest -= TEX_WIDTH - CELL_WIDTH * CELL_SCALE;
		} else {
			dest = dest + (8 - TEX_WIDTH * 8) * CELL_SCALE;
		}
	}

	img_save_pixels("font.png", img, TEX_WIDTH, TEX_HEIGHT, IMG_FMT_GREY8);

	glGenTextures(1, &tex_font);
	glBindTexture(GL_TEXTURE_2D, tex_font);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, TEX_WIDTH, TEX_HEIGHT, 0,
			GL_LUMINANCE, GL_UNSIGNED_BYTE, img);

	fontw = 7;
	fonth = 9;

	glGenBuffers(2, vbo);
	for(i=0; i<2; i++) {
		glBindBuffer(GL_ARRAY_BUFFER, vbo[i]);
		glBufferData(GL_ARRAY_BUFFER, sizeof varr, 0, GL_STREAM_DRAW);
	}
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void text_cleanup(void)
{
	glDeleteBuffers(2, vbo);
	glDeleteTextures(1, &tex_font);
}

void text_begin(void)
{
	nglyphs = 0;
}

void text_end(void)
{
	if(nglyphs <= 0) return;

	glBindTexture(GL_TEXTURE_2D, tex_font);
	glEnable(GL_TEXTURE_2D);

	glBindBuffer(GL_ARRAY_BUFFER, vbo[cur_vbo]);
	glBufferSubData(GL_ARRAY_BUFFER, 0, nglyphs * 4 * sizeof *varr, varr);

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	glVertexPointer(2, GL_FLOAT, sizeof *varr, 0);
	glTexCoordPointer(2, GL_FLOAT, sizeof *varr, (void*)(2 * sizeof(float)));
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glDrawArrays(GL_QUADS, 0, nglyphs * 4);

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	glDisable(GL_TEXTURE_2D);

	nglyphs = 0;
	cur_vbo = (cur_vbo + 1) & 1;
}

#define VERT(vert, vx, vy)	((vert).x = (vx), (vert).y = (vy))
#define TEXC(vert, vu, vv)	((vert).u = (vu), (vert).v = (vv))

void draw_glyph(int x, int y, int c)
{
	float uoffs, voffs;
	int col, row;
	struct vertex *vptr = varr + nglyphs++ * 4;

	if(c < 0x20 || c >= 128) return;

	y = 23 - y;

	VERT(vptr[0], x, y + 1.0 / 9.0);
	VERT(vptr[1], x + 1, y + 1.0 / 9.0);
	VERT(vptr[2], x + 1, y + 1);
	VERT(vptr[3], x, y + 1);

	c -= 0x20;
	row = c / TEX_NCOLS;
	col = c % TEX_NCOLS;

	voffs = row * UVHEIGHT;
	uoffs = col * UVWIDTH;

	TEXC(vptr[0], uoffs, voffs + UVHEIGHT);
	TEXC(vptr[1], uoffs + 7.0 / 8.0 * UVWIDTH, voffs + UVHEIGHT);
	TEXC(vptr[2], uoffs + 7.0 / 8.0 * UVWIDTH, voffs);
	TEXC(vptr[3], uoffs, voffs);

	if(nglyphs >= VB_NGLYPHS) {
		text_end();
	}
}
