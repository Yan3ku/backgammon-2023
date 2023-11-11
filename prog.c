/* ANSI C BACKGAMMON */
#include <curses.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <locale.h>
#include <ncurses.h>

#define TITLE      "The Game of GAMMON by yan3ku v0.1"
#define PLC_SYM(i) (i % 2 ? i < 12 ? "/\\" : "\\/" : "..")
#define nelem(x)   (sizeof(x) / sizeof(*x))

enum {
	OUTERB_START = 6,
	OUTERB_END   = 17,
};

enum { /* spaces */
	PILEH_OFF  = 2,
	PILEV_OFF  = 6,
	GAPH_OFF = 2,
	GAPV_OFF = 3,
	BAR_OFF  = 3
};

enum { /* windows */
	WBOARDER     = 2,
	WBOARD_TOP   = 2,
	WBOARD_LINES = PILEV_OFF * 2 + GAPV_OFF,
	WBOARD_COLS  = PILEH_OFF * 12 + GAPH_OFF * 14 + BAR_OFF,
	WBOARD_BOT   = WBOARD_LINES + WBOARD_TOP + WBOARDER + 1,
	WROOT_LINES  = WBOARD_LINES + WBOARDER + 6,
	WROOT_COLS   = WBOARD_COLS  + WBOARDER + 6,
};

typedef struct {
	int points[24][15];
	int bar[30];
	WINDOW *wroot;
	WINDOW *wboarder;
	WINDOW *wboard;
} Board;

void
setup()
{
	setlocale(LC_ALL, "");
	initscr();
	cbreak();
	noecho();
	start_color();
	curs_set(0);
	keypad(stdscr, 1);
	use_default_colors();
}

/*
 * Here is how the internal board layout looks:
 * HORIZONTAL:
 * <gap> 6*(<plc> <gap>) <bar> <gap> 6*(<plc> <gap>)
 * VERTICAL:
 * 5*<plc> <counter> <gap> <counter> 5*<plc>
 * This is important for "plctoscr" function (place to screen)
 */
void
plctoscr(int plc, int i, int *x, int *y) {
	int col = abs(11 - plc) - (plc > 11);
	int bar = col < 6 ? 0 : BAR_OFF + GAPH_OFF;
	*y = GAPH_OFF + col * (GAPH_OFF + PILEH_OFF) + bar;
	*x = (plc < 12 ? WBOARD_LINES - i - 1 : i);
}

void
boarddrw(Board *board)
{
	size_t i, j;
	int x, y;

	board->wroot    = newwin(WROOT_LINES, WROOT_COLS,
	                        (LINES - WROOT_LINES) / 2,
	                        (COLS  - WROOT_COLS)  / 2);
	board->wboarder = derwin(board->wroot, WBOARD_LINES + 2, WBOARD_COLS + 2, WBOARD_TOP + 1, 0);
	board->wboard   = derwin(board->wboarder, WBOARD_LINES, WBOARD_COLS, 1, 1);
	wborder(board->wboarder, ':', ':', '=', '=', '=', '=', '=', '=');
	mvwprintw(board->wroot, 0, 0, TITLE);

	for (i = 0; i < nelem(board->points); i++)  {
		for (j = 0; j < 6; j++) {
			plctoscr(i, j, &x, &y);
			wattron(board->wboard, A_BOLD);
			mvwprintw(board->wboard, x, y, PLC_SYM(i));
		}
		mvwprintw(board->wroot, i < 12 ? WBOARD_BOT : WBOARD_TOP, y + 1, "%02ld", i + 1);
	}
}

int
main()
{
	Board *board;
	char input[20];

	board = calloc(1, sizeof *board);
	setup();
	boarddrw(board);

	refresh();
	wrefresh(board->wroot);
	wrefresh(board->wboard);
	wrefresh(board->wboarder);
	getch();
	endwin();
	free(board);
	return 0;
}

/* vim: set ts=4 sts=4 sw=4 noet: */
