#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "term.h"

static void clearscr(void);
static void scroll(void);

int cursor_x, cursor_y, cursor_vis, cursor_blink;
unsigned char scrbuf[TERM_COLS * TERM_ROWS];

void term_init(void)
{
	cursor_vis = 1;
	cursor_blink = 0;
	clearscr();
}

void term_cleanup(void)
{
}

static void clearscr(void)
{
	memset(scrbuf, ' ', sizeof scrbuf);
	cursor_x = cursor_y = 0;
}

static void scroll(void)
{
	memmove(scrbuf, scrbuf + TERM_COLS, (TERM_ROWS - 1) * TERM_COLS);
	memset(scrbuf + (TERM_ROWS - 1) * TERM_COLS, ' ', TERM_COLS);
}

static int proc_char(int c)
{
	static int cbuf[8], clen;

	/*
	if(isprint(c)) {
		printf(" %c", c);
	} else {
		printf(" %xh", (unsigned int)c);
	}
	fflush(stdout);
	*/

	if(clen) {
		if(cbuf[0] == 0x1b) {
			switch(clen) {
			case 1:
				if(c != '=') {
					clen = 0;
					return 0;
				}
				break;
			case 2:
				if(c < ' ' || c > '7') {
					clen = 0;
					return 0;
				}
				break;
			case 3:
				if(c < ' ' || c > 'o') {
					clen = 0;
					return 0;
				}
				cursor_x = c - ' ';
				cursor_y = cbuf[2] - ' ';
				clen = 0;
				return 1;
			}
			cbuf[clen++] = c;
			return 0;
		}
	}

	switch(c) {
	case 0:
		break;

	case '\a':
		putchar('\a');
		break;

	case '\b':
		if(cursor_x > 0) {
			cursor_x--;
			return 1;
		}
		break;

	case '\n':
		if(cursor_y >= TERM_ROWS - 1) {
			scroll();
			cursor_y = TERM_ROWS - 1;
		} else {
			cursor_y++;
		}
		cursor_x = 0;
		return 1;

	case '\v':
		if(cursor_y > 0) {
			cursor_y--;
			return 1;
		}
		break;

	case 0xc:	/* FF */
		if(cursor_x < TERM_COLS - 1) {
			cursor_x++;
			return 1;
		}
		break;

	case '\r':
		cursor_x = 0;
		return 1;

	case 0xe:	/* SO TODO */
		break;
	case 0xf:	/* SI TODO */
		break;

	case 0x1a:	/* SUB */
		clearscr();
		return 1;

	case 0x1b:	/* ESC */
		cbuf[clen++] = c;
		break;

	case 0x1e:	/* RS */
		cursor_x = cursor_y = 0;
		return 1;

	default:
		scrbuf[cursor_y * TERM_COLS + cursor_x] = c;
		if(++cursor_x >= TERM_COLS) {
			proc_char('\n');
		}
		return 1;
	}

	return 0;
}

int term_proc(char *buf, int sz)
{
	int redisp = 0;

	while(sz-- > 0) {
		redisp |= proc_char(*buf++);
	}

	return redisp;
}
