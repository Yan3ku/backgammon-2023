#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <ncurses.h>

/* Here is how the internal board layout looks:
 * HORIZONTAL:
 * <gap> 6*(<plc> <gap>) <bar> <gap> 6*(<plc> <gap>)
 * VERTICAL:
 * 5*<plc> <counter> <gap> <counter> 5*<plc>
 * This is important for "plctoscr" function (place to screen)
 */

#define TITLE     "The Game of GAMMON by yan3ku v0.1"

#define PLC_SYM(i) (i % 2 ? i < 12 ? "/\\" : "\\/" : "..")
#define nelem(x) (sizeof(x) / sizeof(*x))

enum {
	BOARD_PLC = 24,
	OUTERB_START = 6,
	OUTERB_END = 17,
	CHECK_COUNT = 15,
	/* offsets */
	PLC_OFF = 2,  /* place width    */
	GAPH_OFF = 2, /* horizontal gap */
	GAPV_OFF = 3, /* vertical gap   */
	BAR_OFF = 3,

	WBOARD_ROWS = 6 * 2 + GAPV_OFF,
	WBOARD_COLS = PLC_OFF * 12 + GAPH_OFF * 14 + BAR_OFF,
};

typedef struct {
	int points[BOARD_PLC][CHECK_COUNT];
	int bar[CHECK_COUNT];
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

void
plctoscr(int plc, int i, int *x, int *y) {
	int col = abs(11 - plc) - (plc > 11);
	int bar = col < 6 ? 0 : BAR_OFF + GAPH_OFF;
	*y = GAPH_OFF + col * (GAPH_OFF + PLC_OFF) + bar;
	*x = (plc < 12 ? WBOARD_ROWS - i - 1 : i);
}

void
boarddrw(Board *board)
{
	int i, j;
	int x, y;

	board->wroot    = newwin(
			WBOARD_ROWS + 2 + 6,       WBOARD_COLS + 2 + 6,
			(LINES - WBOARD_ROWS - 8) / 2,  (COLS - WBOARD_COLS - 8) / 2
			);
	board->wboarder = derwin(board->wroot, WBOARD_ROWS + 2, WBOARD_COLS + 2, 3, 0);
	board->wboard   = derwin(board->wboarder, WBOARD_ROWS, WBOARD_COLS, 1, 1);
	wborder(board->wboarder, ':', ':', '=', '=', '=', '=', '=', '=');
	mvwprintw(board->wroot, 0, 0, TITLE);

	for (i = 0; i < BOARD_PLC; i++)  {
		for (j = 0; j < 6; j++) {
			plctoscr(i, j, &x, &y);
			wattron(board->wboard, A_BOLD);
			mvwprintw(board->wboard, x, y, PLC_SYM(i));
		}
		mvwprintw(board->wroot, i > 11 ? 2 : WBOARD_ROWS + 5, y + 1, "%02d", i + 1);
	}
}

int
main()
{
	Board *board;

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
