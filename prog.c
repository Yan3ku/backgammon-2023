/* ANSI C GAMMON */
#include <curses.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <locale.h> /* extend ASCII ncurses */
#include <ncursesw/curses.h>

#define TITLE            "The Game of GAMMON by yan3ku v0.1"
#define ROLL_PROMPT(p)   "%s roll> ", ROLL_STR[(p)]
#define OTHER_PLAYER(p)   ((p) == ME ? YOU : (p) == YOU ? ME : EMPTY)
#define nelem(x)         (sizeof(x) / sizeof(*x))
#define TERM_SIZE_ERR    "Terminal window too small!"
#define PLAYER_SYM(pl)   ((pl) == ME ? "░░" : "▓▓")
#define PLAYER_DIR(pl)   ((pl) == ME ? "low to high" : "high to low")
#define UNUSED(p)        (void)(p)
#define INPUT_LEN        5
#define CMD_SIZ          5
#define ERR_DISP_TIME    2000

typedef enum {
	ME,
	YOU,
	EMPTY,
} Player;

typedef enum {
	START,
	RUN,
	END,
} State;

typedef enum {
	INDEX_UNBOUND_ERR = 2, /* first 0 and 1 are indexes to cube array */
	CMD_NAME_ERR,
	INVALID_INPUT_ERR,
	INPUT_ERR,
	MOVE_DIR_ERR,
	IS_NOT_PLAYER_ERR,
	CANT_BEAT_ERR,
	NON_EMPTY_BAR_ERR,
	NO_CHECKERS_BAR_ERR,
	IS_NOT_HOME_ERR,
	NO_MOVE_ROLLED_ERR,
} MoveError;


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
	PROMPT_LINES = 2,
	HOME_COLS    = 7,
	BOARD_LINES  = PILE_LINES * 2 + PILE_GAP_LINES + BOARDER,
	BOARD_COLS   = PILE_COLS * 12 + PILE_GAP_COLS * 14 + BAR_COLS + BOARDER,
	ROOT_LINES   = BOARD_LINES + PROMPT_LINES + 5,
	ROOT_COLS    = BOARD_COLS  + HOME_COLS,
};

typedef struct {
	char val;
	char use;
} Cube;

typedef struct {
	Player pl;
	int count;
} Place;

typedef struct {
	Place places[24];
	int bar[2];
	int home[2];
	Player turn;
	State state;
	Cube cube[2];
	WINDOW *root;
	WINDOW *board;
	WINDOW *roll[2];
	WINDOW *prompt;
	WINDOW *score;
	WINDOW *whome;
} Game;

#define IS_HOME(pl, plc) (pl == ME ? plc >= 18 : plc <= 5)
#define START_PLC(pl)    (pl == ME ? -1 : 24)

const char *ERR_STR[] = {
	[INDEX_UNBOUND_ERR]   = "No such place exist!",
	[INVALID_INPUT_ERR]   = "Wrong input! Usefull words: BAR, HOME, OUT",
	[MOVE_DIR_ERR]        = "Wrong movement direction!",
	[IS_NOT_PLAYER_ERR]   = "No checkers to move there!",
	[CANT_BEAT_ERR]       = "You can't beat the opposing player!",
	[NO_MOVE_ROLLED_ERR]  = "No such move was rolled!",
	[NON_EMPTY_BAR_ERR]   = "You have checkers on bar!",
	[NO_CHECKERS_BAR_ERR] = "No checkers on bar!",
	[IS_NOT_HOME_ERR]     = "Destination must be home!",
};

const char *ROLL_STR[] = {"My", "Your"};

void
setup()
{
	srand(time(NULL));
	setlocale(LC_ALL, "");
	initscr();
	cbreak();
	noecho();
	start_color();
	keypad(stdscr, TRUE);
	use_default_colors();
}

Game *
gamecreate()
{
	Game *game;
	size_t i;

	game = calloc(1, sizeof *game);
	for (i = 0; i < 12; i++) {
		game->places[i].pl = EMPTY;
	}

	/*
	   game->places[0]  = (Place){.pl = ME,  .count = 2};
	   game->places[5]  = (Place){.pl = YOU, .count = 5};
	   game->places[7]  = (Place){.pl = YOU, .count = 3};
	   game->places[11] = (Place){.pl = ME,  .count = 5};
	*/

	game->places[0]  = (Place){.pl = ME,  .count = 1};
	game->places[1]  = (Place){.pl = YOU, .count = 1};
	game->places[2]  = (Place){.pl = ME, .count = 1};
	game->places[3]  = (Place){.pl = YOU, .count = 1};
	game->places[4]  = (Place){.pl = YOU, .count = 1};
	game->places[7]  = (Place){.pl = YOU, .count = 1};
	game->places[11] = (Place){.pl = ME,  .count = 1};
	game->bar[ME] = 3;
	game->bar[YOU] = 3;
	game->home[YOU] = 12;
	game->home[ME] = 7;

	for (i = 0; i < 12; i++) {
		game->places[23 - i].pl = OTHER_PLAYER(game->places[i].pl);
		game->places[23 - i].count = game->places[i].count;
	}

	game->state = START;
	return game;
}

/*
 * Here is how the internal board layout looks:
 * HORIZONTAL:
 * <gap> 6*(<plc> <gap>) <bar> <gap> 6*(<plc> <gap>)
 * VERTICAL:
 * 5*<plc> <counter> <gap> <counter> 5*<plc>
 * +2 for outside border
 * The below calculations are based on that.
 */
void
plctoscr(int i, int j, int *y, int *x)
{
	int col = abs(11 - i) - (i > 11);
	int bar = col < 6 ? 0 : BAR_COLS + PILE_GAP_COLS;
	*x = PILE_GAP_COLS + col * (PILE_GAP_COLS + PILE_COLS) + bar + 1;
	*y = (i < 12 ? BOARD_LINES - j - 2 : j + 1);
}

const char *
plctosym(const Place *plc, int i, int j)
{
	static char str[3];
	if (j == 5 && plc[i].count > 5) {
		sprintf(str, "%02hu", (short)(plc[i].count - 5));
		return str;
	}
	if (plc[i].count > j && plc[i].pl != EMPTY)
		return PLAYER_SYM(plc[i].pl);
	return i % 2 ? (i < 12 ? "/\\" : "\\/") : "..";
}

void
boarddrw(Game *game)
{
	size_t i, j;
	int x, y;

	for (i = 0; i < 3; i++) /* bar lines */
		mvwvline(game->board, 1, BOARD_COLS / 2 - i + 1, '|', BOARD_LINES - 2);
	mvwprintw(game->board, BOARD_LINES / 2, BOARD_COLS / 2 - 2, "[BAR]");

	for (i = 0; i < 2; i++) { /* bar checkers */
		for (j = 0; j < (size_t)game->bar[i] && j < 5; j++) {
			y = (i == YOU ? BOARD_LINES - 2 - j : j + 1);
			mvwaddstr(game->board, y, BOARD_COLS / 2 - (i == YOU ? 1 : 0),
			          PLAYER_SYM(i));
		}
	}

	for (i = 0; i < 2; i++) { /* home */
		for (j = 0; j < (size_t)game->home[i]; j++) {
			y = (i == YOU ? BOARD_LINES - 2 - j % 5 : j % 5 + 1);
			mvwaddstr(game->whome, y, j / 5 * 2, PLAYER_SYM(i));
		}
	}
	mvwprintw(game->whome, BOARD_LINES / 2, 0, "[HOME]");

	for (i = 0; i < nelem(game->places); i++)  { /* places */
		for (j = 0; j < 6; j++) {
			plctoscr(i, j, &y, &x);
			mvwaddstr(game->board, y, x, plctosym(game->places, i, j));
		}
	}

	wnoutrefresh(game->whome);
	wnoutrefresh(game->board);
}

void
gameinitdrw(Game *game)
{
	size_t i, j, boardendy;
	int x, y;

	if (COLS < ROOT_COLS || LINES < ROOT_LINES) {
		mvprintw(LINES / 2, (COLS  - strlen(TERM_SIZE_ERR)) / 2, TERM_SIZE_ERR);
		curs_set(0);
		refresh();
		return;
	}

	game->root  = newwin(ROOT_LINES, ROOT_COLS,
	                     (LINES - ROOT_LINES) / 2,
	                     (COLS  - ROOT_COLS)  / 2);
	game->board = derwin(game->root, BOARD_LINES, BOARD_COLS, 3, 0);
	boardendy   = getpary(game->board) + getmaxy(game->board);

	plctoscr(13, 0, &y, &x); /* 13 place pos */
	game->roll[0] = derwin(game->board, 1, 18, BOARD_LINES / 2, x);
	plctoscr(19, 0, &y, &x);
	game->roll[1] = derwin(game->board, 1, 18, BOARD_LINES / 2, x);
	game->prompt  = derwin(game->root, PROMPT_LINES, BOARD_COLS, boardendy + 2, 0);
	wborder(game->board, ':', ':', '=', '=', ':', ':', ':', ':');
	mvwaddstr(game->root, 0, 0, TITLE);
	mvwaddstr(game->prompt, 0, 0, TITLE);
	game->whome = derwin(game->root, BOARD_LINES, HOME_COLS, getpary(game->board), BOARD_COLS);
	mvwhline(game->whome, 0, 0, '=', HOME_COLS);
	mvwhline(game->whome, BOARD_LINES - 1, 0, '=', HOME_COLS);
	mvwvline(game->whome, 0, HOME_COLS - 1, ':', BOARD_LINES);

	boarddrw(game);

	for (i = 0, j = 0; i < nelem(game->places); i++) {
		plctoscr(i, j, &y, &x);
		mvwprintw(game->root, i < 12 ? boardendy : getpary(game->board) - 1, x, "%2ld", i + 1);
	}

	wnoutrefresh(stdscr);
	wnoutrefresh(game->root);
	wnoutrefresh(game->whome);
	wnoutrefresh(game->prompt);
	wnoutrefresh(game->board);
	doupdate();
}

void
roll(Cube cube[2])
{
	int i;
	for (i = 0; i < 2; i++) cube[i].val = rand() % 6 + 1;
	cube[0].use = cube[1].use = (cube[0].val == cube[1].val ? 2 : 1);
}

#define rolldrw(game, player, str, ...) do {                                   \
	mvwprintw(game->roll[player], 0, 0, ROLL_PROMPT(player));                  \
	wprintw(  game->roll[player], str, __VA_ARGS__);                           \
	wnoutrefresh( game->roll[player]);                                         \
} while (0)

int /* newline consuming alternative to wget */
wread(WINDOW *win, char *input, int maxlen)
{
	int ch;
	*input = '\0';
	char *beg = input;
	doupdate();
	keypad(win, TRUE);
	while ((ch = wgetch(win)) != '\n') {
		switch (ch) {
		case KEY_BACKSPACE:
			if (input - beg <= 0) continue;
			wmove(win, getcury(win), getcurx(win) - 1);
			wdelch(win);
			input--;
			break;
		default:
			if (input - beg >= maxlen) continue;
			waddch(win, ch);
			*(input++) = ch;
			break;
		}
	}
	*input = '\0';
	return input - beg;
}

int
readprompt(Game *game, char *start, char *end)
{
	int i, j;
	*start = '\0';
	*end = '\0';
	mvwprintw(game->prompt, 0, 0, "You %s have ", PLAYER_SYM(game->turn));
	for (i = 0; i < 2; i++)
		for (j = 0; j < game->cube[i].use; j++)
			wprintw(game->prompt, "[%d] ", game->cube[i].val);
	wprintw(game->prompt, "left, moving from %s", PLAYER_DIR(game->turn));

	for (;;) {
		wmove(game->prompt, 1, 0);
		wclrtoeol(game->prompt);
		mvwprintw(game->prompt, 1, 0, "Move from? ");
		wnoutrefresh(game->prompt);
		wread(game->prompt, start, INPUT_LEN);
		if (!strcmp(start, "END")) {
			mvwprintw(game->prompt, 1, 0, "End turn (ok/no)? ");
			wread(game->prompt, start, INPUT_LEN);
			if (!strcmp(start, "ok")) return 0;
		} else break;
	};

	wprintw(game->prompt, " To? ");
	wread(game->prompt, end, INPUT_LEN);
	wclear(game->prompt);
	wnoutrefresh(game->prompt);
	return 1;
}

size_t
isvalidmv(const Game *game, int from, int to, int isbar)
{
	size_t i;
	int mv;
	if (isbar) {
		if (game->bar[game->turn] == 0)                 return NO_CHECKERS_BAR_ERR;
		if (!IS_HOME(OTHER_PLAYER(game->turn), to))     return IS_NOT_HOME_ERR;
	} else {
		if (game->bar[game->turn] != 0)                 return NON_EMPTY_BAR_ERR;
		if (from < 0 || from > 23 || to < 0 || to > 23) return INDEX_UNBOUND_ERR;
		if (game->places[from].pl != game->turn)        return IS_NOT_PLAYER_ERR;
	}

	if ((game->turn == ME  && from > to) ||
        (game->turn == YOU && from < to)) return MOVE_DIR_ERR;

	if (game->places[to].pl == OTHER_PLAYER(game->turn) &&
	    game->places[to].count > 1) return CANT_BEAT_ERR;

	for (mv = abs(from - to), i = 0; i < nelem(game->cube); i++)
		if (game->cube[i].use && game->cube[i].val == mv) return i;
	return NO_MOVE_ROLLED_ERR;
}

int
parseinput(char *input, size_t *idx, char *keyword, int *iskeyword)
{
	char *endptr;

	*idx = strtol(input, &endptr, 10) - 1;
	if (input == endptr) {
		if (!(*iskeyword = !strcmp(input, keyword))) return INVALID_INPUT_ERR;
	} else if (*endptr != '\0') return INVALID_INPUT_ERR;
	return 0;
}

int
mvchecker(Game *game, char *start, char *end)
{
	size_t from, to, idx;
	int isbar, ishome, err;
	int sign;

	isbar = ishome = 0;
	if ((err = parseinput(start, &from, "BAR", &isbar))) return err;
	if (isbar) from = START_PLC(game->turn);

	sign = (*end == '-' ? -1 : (*end == '+' ? 1 : 0)); /* if sign skip it */
	if (sign) end++;

	if ((err = parseinput(end, &to, "HOME", &ishome))) return err;
	if (ishome) to = -1;
	if (sign) to = from + sign * (to + 1);

	mvwprintw(game->root, 0, 0, "[%s] [%s] [%ld] [%ld]", start, end, from, to);
	wrefresh(game->root);
	if ((idx = isvalidmv(game, from, to, isbar)) >= nelem(game->cube)) return idx;
	if (game->places[to].pl == OTHER_PLAYER(game->turn)) { /* beating */
		game->bar[OTHER_PLAYER(game->turn)] += 1;
		game->places[to].count = 0;
	}
	game->places[to].pl = game->turn;
	game->places[to].count++;
	if (isbar) game->bar[game->turn]--;
	else game->places[from].count--;
	return idx;
}


/*
 * ROLL 2 3 4 5 - roll and new turn
 * BAR  22      - moves from bar to position
 * MOVE 22 23   - moves from place to place
*/
int
executecmd(Game *game, char *cmd, int *args)
{
	if (strcmp("ROLL", cmd)) {

	} else if (strcmp("BAR", cmd)) {

	} else if (strcmp("MOVE", cmd)) {
		
	} else {
		return CMD_NAME_ERR;
	}
	return 0;
}


int
main()
{
	Game *game;
	char start[INPUT_LEN], end[INPUT_LEN];
	size_t i;

	setup();
	game = gamecreate();
	for (;;) {
		switch (game->state) {
		case START:
			gameinitdrw(game);
			do { roll(game->cube); } while (game->cube[0].val == game->cube[1].val);
			rolldrw(game, ME,  "[%d]", game->cube[ME].val);
			rolldrw(game, YOU, "[%d]", game->cube[YOU].val);
			game->turn = game->cube[ME].val > game->cube[YOU].val ? ME : YOU;
			game->cube[0].val = 1;
			game->cube[1].val = 2;
			game->state = RUN;
			goto READ;
		case RUN:
			roll(game->cube);
			rolldrw(game, game->turn, "[%d] [%d]", game->cube[0].val, game->cube[1].val);
		READ:
			for (;;) {
				if (!readprompt(game, start, end)) break;
				if ((i = mvchecker(game, start, end)) < nelem(game->cube)) {
					boarddrw(game);
					game->cube[i].use--;
					if (!game->cube[0].use && !game->cube[1].use) break;
					continue;
				}
				/* INVALID MOVE */
				curs_set(0);
				wclear(game->prompt);
				mvwprintw(game->prompt, 0, 0, ERR_STR[i], start, end);
				wrefresh(game->prompt);
				napms(ERR_DISP_TIME);
				curs_set(1);
				wclear(game->prompt);
			}
			wclear(game->roll[game->turn]);
			wnoutrefresh(game->roll[game->turn]);
			game->turn = OTHER_PLAYER(game->turn);
			break;
		case END:
			goto END;
		}
	}
END:
	endwin();
	free(game);
	return 0;
}

/* vim: set ts=4 sts=4 sw=4 noet: */
