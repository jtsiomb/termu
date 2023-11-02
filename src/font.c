#include <stdio.h>
#include <string.h>
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include "font.h"

/* ADM3a 5x7 glyphs, displayed in a 7x9 matrix */

#define ROM_CHARS	128

#define TEX_NCOLS	16
#define TEX_NROWS	(ROM_CHARS / TEX_NCOLS)

#define TEX_WIDTH	(TEX_NCOLS * 8)
#define TEX_HEIGHT	(TEX_NROWS * 8)

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


static int transform(int c)
{
	if(c >= 0x60) {
		return ((~c) & 0x7f) | 0x40;
	}
	return c & 0x3f;
}

void text_init(void)
{
	int i, j, k, glyph;
	unsigned char img[TEX_WIDTH * TEX_HEIGHT];
	unsigned char *src, *dest = img;

	memset(img, 0, sizeof img);

	for(i=0; i<ROM_CHARS; i++) {
		glyph = transform(i + 0x20);
		src = rom_upper + glyph * 8;
		for(j=0; j<8; j++) {
			unsigned char row = (*src++ & 0x1f) << 2;
			for(k=0; k<8; k++) {
				dest[k] = (row >> (7 - k)) & 1 ? 0xff : 0;
			}
			dest += TEX_WIDTH;
		}

		if((i & 0xf) == 0xf) {
			dest -= TEX_WIDTH - 8;
		} else {
			dest = dest + 8 - TEX_WIDTH * 8;
		}

	}
	/*{
		FILE *fp = fopen("font.ppm", "wb");
		if(fp) {
			fprintf(fp, "P5\n%d %d\n255\n", TEX_WIDTH, TEX_HEIGHT);
			fwrite(img, 1, TEX_WIDTH * TEX_HEIGHT, fp);
			fclose(fp);
		}
	}*/

	glGenTextures(1, &tex_font);
	glBindTexture(GL_TEXTURE_2D, tex_font);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
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
