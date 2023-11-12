/* ANSI C GAMMON */
/* note: x: columns, y: rows for ncurses */
#include <curses.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <locale.h> /* extend ASCII ncurses */
#include <ncurses.h>

#define TITLE            "The Game of GAMMON by yan3ku v0.1"
#define ROLL_PR(p)       "%s roll> ", ROLL_STR[p - 1]
#define PLC_SYM_(i)      (i % 2 ? i < 12 ? "/\\" : "\\/" : "..")
#define PLC_SYM(c, i, j) (c[i][j] == ME ? "░░" : c[i][j] == YOU ? "▓▓" : PLC_SYM_(i))
#define SWAP_PL(p)       (p == ME ? YOU : p == YOU ? ME : EMPTY)
#define nelem(x)         (sizeof(x) / sizeof(*x))


typedef enum { /* players */
	EMPTY,
	ME,
	YOU,
} Player;

typedef enum {
	START,
	RUN,
	END,
} State;

const char *ROLL_STR[] = {"My", "Your"};

enum {
	OUTERB_START = 6,
	OUTERB_END   = 17,
};


enum { /* spaces */
	PILE_COLS      = 2,
	PILE_LINES     = 6,
	PILE_GAP_LINES = 3,
	PILE_GAP_COLS  = 3,
	BAR_COLS       = 3
};


enum { /* windows */
	BOARDER      = 2,
	BOARD_LINES  = PILE_LINES * 2 + PILE_GAP_LINES + BOARDER,
	BOARD_COLS   = PILE_COLS * 12 + PILE_GAP_COLS * 14 + BAR_COLS + BOARDER,
	PROMPT_LINES = 2,
	ROOT_LINES   = BOARD_LINES + PROMPT_LINES + 5,
	HOME_COLS    = 6,
	ROOT_COLS    = BOARD_COLS  + HOME_COLS,
};


typedef struct {
	Player points[24][15];
	Player bar[30];
	Player turn;
	State state;
	int cube[2];
	WINDOW *root;
	WINDOW *board;
	WINDOW *roll[2];
	WINDOW *prompt;
	WINDOW *score;
	WINDOW *home;
} Game;

const int GAME_TEMPLATE[12][15] = { /* beginning game state */
	[0]  = {ME, ME},
	[5]  = {YOU, YOU, YOU, YOU, YOU},
	[7]  = {YOU, YOU, YOU},
	[11] = {ME, ME, ME, ME, ME},
};


void
setup()
{
	srand(time(NULL));
	setlocale(LC_ALL, "");
	initscr();
	cbreak();
	noecho();
	start_color();
	curs_set(0);
	keypad(stdscr, TRUE);
	use_default_colors();
}

/*
 * Here is how the internal board layout looks:
 * HORIZONTAL:
 * <gap> 6*(<plc> <gap>) <bar> <gap> 6*(<plc> <gap>)
 * VERTICAL:
 * 5*<plc> <counter> <gap> <counter> 5*<plc>
 * +2 for outside border
 */
void
plctoscr(int plc, int i, int *y, int *x) {
	int col = abs(11 - plc) - (plc > 11);
	int bar = col < 6 ? 0 : BAR_COLS + PILE_GAP_COLS;
	*x = PILE_GAP_COLS + col * (PILE_GAP_COLS + PILE_COLS) + bar + 1;
	*y = (plc < 12 ? BOARD_LINES - i - 2 : i + 1);
}

void
gamedrw(Game *game)
{
	size_t i, j, boardendy;
	int x, y;

	game->root  = newwin(ROOT_LINES, ROOT_COLS,
	                     (LINES - ROOT_LINES) / 2,
	                     (COLS  - ROOT_COLS)  / 2);
	game->board = derwin(game->root, BOARD_LINES, BOARD_COLS, 3, 0);
	boardendy   = getpary(game->board) + getmaxy(game->board);

	plctoscr(13, 0, &y, &x); /* 13 pile pos */
	game->roll[0] = derwin(game->board, 1, 18, BOARD_LINES / 2, x);
	plctoscr(19, 0, &y, &x);
	game->roll[1] = derwin(game->board, 1, 18, BOARD_LINES / 2, x);
	game->prompt  = derwin(game->root, PROMPT_LINES, BOARD_COLS, boardendy + 2, 0);
	wborder(game->board, ':', ':', '=', '=', ':', ':', ':', ':');
	mvwprintw(game->root, 0, 0, TITLE);
	mvwprintw(game->prompt, 0, 0, TITLE);


	for (i = 0; i < 3; i++) /* bar */
		mvwvline(game->board, 1, BOARD_COLS / 2 - i + 1, '|', BOARD_LINES - 2);
	mvwprintw(game->board, BOARD_LINES / 2, BOARD_COLS / 2 - 2, "[BAR]");

	for (i = 0; i < nelem(game->points); i++)  { /* points */
		for (j = 0; j < 6; j++) {
			plctoscr(i, j, &y, &x);
			mvwprintw(game->board, y, x, PLC_SYM(game->points, i, j));
		}
		mvwprintw(game->root, i < 12 ? boardendy : getpary(game->board) - 1, x, "%2ld", i + 1);
	}
	mvwprintw(game->root, BOARD_LINES / 2 + getpary(game->board), BOARD_COLS, "[HOME]");

	wnoutrefresh(stdscr);
	wnoutrefresh(game->root);
	wnoutrefresh(game->prompt);
	wnoutrefresh(game->board);
}

void
roll(int cube[2])
{
	int i;
	for (i = 0; i < 2; i++) cube[i] = rand() % 6;
}

#define rolldrw(game, player, str, ...) do {                                   \
	mvwprintw(game->roll[player - 1], 0, 0, ROLL_PR(player));                 \
	wprintw(  game->roll[player - 1], str, __VA_ARGS__);                      \
	wnoutrefresh( game->roll[player - 1]);                                    \
} while (0)

void
readprompt(Game *game)
{
	mvwprintw(game->prompt, 0, 0, "You ▓▓ have [%d] [%d] left, moving from high to low.", game->cube[0], game->cube[1]);
	mvwprintw(game->prompt, 1, 0, "Move from? ");
	wnoutrefresh(game->prompt);
}

void
updatescr(Game *game)
{
}

void
gamesetup(Game *game)
{
	size_t i, j;

	memcpy(game->points, GAME_TEMPLATE, sizeof(GAME_TEMPLATE));
	for (i = 0; i < 12; i++) {
		for (j = 0; j < 5; j++)
			game->points[23 - i][j] = SWAP_PL(game->points[i][j]);
	}
	game->state = START;
}

int
main()
{
	Game *game;

	game = calloc(1, sizeof *game);
	setup();

	for (;;) {
		switch (game->state) {
		case START: /* FALLTHROUGH */
			gamesetup(game);
			gamedrw(game);
			doupdate();
			roll(game->cube);
			rolldrw(game, ME,  "[%d]", game->cube[0]);
			rolldrw(game, YOU, "[%d]", game->cube[1]);
			game->turn = game->cube[0] > game->cube[1] ? ME : YOU;
			doupdate();
			getch();
			wclear(game->roll[0]);
			wclear(game->roll[1]);
			wnoutrefresh(game->roll[0]);
			wnoutrefresh(game->roll[1]);
			doupdate();
			game->state = RUN;
		case RUN:
			roll(game->cube);
			rolldrw(game, game->turn, "[%d] [%d]", game->cube[0], game->cube[1]);
			doupdate();
			/*readprompt(game);*/
			updatescr(game);
			getch();
			break;
		case END:
			goto end;
		}
	}
end:

	endwin();
	free(game);
	return 0;
}

/* vim: set ts=4 sts=4 sw=4 noet: */
