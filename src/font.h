#ifndef FONT_H_
#define FONT_H_

extern int fontw, fonth;
extern unsigned int tex_font;

void text_init(void);
void text_cleanup(void);

void text_begin(void);
void draw_glyph(int x, int y, int c);
void text_end(void);

#endif	/* FONT_H_ */
