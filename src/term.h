#ifndef TERM_H_
#define TERM_H_

#define TERM_COLS	80
#define TERM_ROWS	24

extern int cursor_x, cursor_y, cursor_vis, cursor_blink;
extern unsigned char scrbuf[TERM_COLS * TERM_ROWS];

void term_init(void);
void term_cleanup(void);

int term_proc(char *buf, int sz);

#endif	/* TERM_H_ */
