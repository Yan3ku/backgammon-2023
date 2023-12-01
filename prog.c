/* ANSI C GAMMON */
#include <curses.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <locale.h> /* extend ASCII ncurses */
#include <ncursesw/curses.h>

#define PLAYER_COUNT     2
#define DICE_COUNT       2
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
#define IS_HOME(pl, plc) ((pl) == ME ? (plc) >= 18 : (plc) <= 5)
#define IS_EXE_ERR(x)    ((x) >= DICE_COUNT)

/* not used as indexes but for calculations of distance */
#define START_PLC(pl)    ((pl) == ME ? -1 : 24)
#define BEAROFF_PLC(pl)  ((pl) == ME ? 24 : -1)
#define FST_ARG(cmd, pl) ((cmd).args[0].type == BAR_ARG ?  START_PLC(pl)   : (cmd).args[0].id)
#define SND_ARG(cmd, pl) ((cmd).args[1].type == HOME_ARG ? BEAROFF_PLC(pl) : (cmd).args[1].id)


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
	INDEX_UNBOUND_ERR = PLAYER_COUNT, /* if function returns player it's not error */
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
	CANT_BEAROFF_ERR,
} ExeErr;


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
} Dice;

typedef enum {
	BAR_ARG,
	HOME_ARG,
	PLC_ARG,
} CmdArgType;

typedef struct {
	CmdArgType type;
	int id;
} CmdArg;

const char *ARG_TYPE2STR[] = {
	[BAR_ARG] = "BAR",
	[HOME_ARG] = "HOME",
};

typedef struct {
	enum {
		MOVE_CMD,
		ROLL_CMD,
	} type;
	CmdArg args[4];
} Cmd;

typedef struct {
	Player pl;
	int count;
} Place;

typedef struct {
	Place places[24];
	int bar[PLAYER_COUNT];
	int home[PLAYER_COUNT];
	Player turn;
	State state;
	Dice dice[DICE_COUNT];
	WINDOW *root;
	WINDOW *board;
	WINDOW *roll[PLAYER_COUNT];
	WINDOW *prompt;
	WINDOW *score;
	WINDOW *whome;
} Game;

const char *ERR_STR[] = {
	[INDEX_UNBOUND_ERR]   = "No such place exist!",
	[INVALID_INPUT_ERR]   = "Wrong input! Usefull words: BAR, HOME, OUT",
	[MOVE_DIR_ERR]        = "Wrong movement direction!",
	[IS_NOT_PLAYER_ERR]   = "No checkers to move there!",
	[CANT_BEAT_ERR]       = "You can't beat the opposing player!",
	[NO_MOVE_ROLLED_ERR]  = "No such move was rolled!",
	[NON_EMPTY_BAR_ERR]   = "You have checkers on bar!",
	[NO_CHECKERS_BAR_ERR] = "No checkers on bar!",
	[IS_NOT_HOME_ERR]     = "Position must be home!",
	[CANT_BEAROFF_ERR]    = "You can't bearoff right now!"
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

	game->places[0]  = (Place){.pl = YOU,  .count = 5};
	game->places[1]  = (Place){.pl = YOU, .count = 10};
	game->bar[ME] = 0;
	game->bar[YOU] = 0;
	game->home[YOU] = 0;
	game->home[ME] = 0;

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
roll(Dice dice[2])
{
	int i;
	for (i = 0; i < 2; i++) dice[i].val = rand() % 6 + 1;
	dice[0].use = dice[1].use = (dice[0].val == dice[1].val ? 2 : 1);
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
		for (j = 0; j < game->dice[i].use; j++)
			wprintw(game->prompt, "[%d] ", game->dice[i].val);
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
			if (!strcmp(start, "ok")) return 1;
		} else break;
	};

	wprintw(game->prompt, " To? ");
	wread(game->prompt, end, INPUT_LEN);
	wclear(game->prompt);
	wnoutrefresh(game->prompt);
	return 0;
}

int
canbearoff(const Game *game)
{
	int count, i;
	for (count = 0, i = 0; i < nelem(game->places); i++) {
		if (IS_HOME(game->turn, i) && game->places[i].pl == game->turn) {
			count += game->places[i].count;
		}
	}
	count += game->home[game->turn];
	return count == 15;
}


int
findmove(const Dice dice[2], int moveabs, int (*predicate)(int, int))
{

	int i = 0;
	for (i = 0; i < DICE_COUNT; i++)
		if (dice[i].use && predicate(dice[i].val, moveabs))
			return i;
	return NO_MOVE_ROLLED_ERR;
}

/* predicates */
int eq(int a, int b) { return a == b; }
int geq(int a, int b) { return a >= b; }

int
islastchecker(const Game *game, int pos)
{
	int dir, i;
	dir = (game->turn ? -1 : +1);
	for (i = BEAROFF_PLC(game->turn) - 6 * dir; 0 < i && i < 24; i += dir) {
		if (game->places[i].pl == game->turn) { return i == pos; }
	}
	return -1;
}

ExeErr
isvalidcmd(const Game *game, Cmd *cmd)
{
	int idx, from, to;

	from = FST_ARG(*cmd, game->turn);
	to   = SND_ARG(*cmd, game->turn);

	if (cmd->args[0].type == BAR_ARG) {
		if (game->bar[game->turn] == 0) { return NO_CHECKERS_BAR_ERR; }
		if (cmd->args[1].type != PLC_ARG || !IS_HOME(OTHER_PLAYER(game->turn), cmd->args[1].id)) {
			return IS_NOT_HOME_ERR;
		}
	} else if (cmd->args[1].type == HOME_ARG) {
		if (!canbearoff(game)) { return CANT_BEAROFF_ERR; }
		if (!IS_HOME(game->turn, cmd->args[0].id)) { return IS_NOT_HOME_ERR; }
		if (game->places[from].pl != game->turn) { return IS_NOT_PLAYER_ERR; }
	} else {
		if (game->bar[game->turn] != 0) { return NON_EMPTY_BAR_ERR; }
		if (from < 0 || from > 23 || to < 0 || to > 23) { return INDEX_UNBOUND_ERR; }
		if (game->places[from].pl != game->turn) { return IS_NOT_PLAYER_ERR; }
	}

	if ((game->turn == ME  && from > to) ||
	    (game->turn == YOU && from < to)) {
		return MOVE_DIR_ERR;
	} 

	if (game->places[to].pl == OTHER_PLAYER(game->turn) && game->places[to].count > 1) {
		return CANT_BEAT_ERR;
	} 

	if (!IS_EXE_ERR(idx = findmove(game->dice, abs(from - to), eq))) { return idx; }
	if (cmd->args[1].type != HOME_ARG) { return NO_MOVE_ROLLED_ERR; }
	/* 
	 * there is special rule that says:
	 * if the roll is greater than the distance from last checker to home you can bearoff it
	 */
	if (islastchecker(game, from) && !IS_EXE_ERR(idx = findmove(game->dice, abs(from - to), geq))) {
			return idx;
	}
	return NO_MOVE_ROLLED_ERR;
}

int
parseinput(char *input, CmdArg *arg, CmdArgType type)
{
	char *endptr;

	arg->id = strtol(input, &endptr, 10) - 1;
	arg->type = PLC_ARG;
	if (input == endptr) {
		if ((!strcmp(input, ARG_TYPE2STR[type]))) { arg->type = type; }
		else { return INVALID_INPUT_ERR; }
	} else if (*endptr != '\0') { return INVALID_INPUT_ERR; }
	return 0;
}

int
parsecmdfromprompt(Cmd *cmd, const Game *game, char *start, char *end)
{
	int *from, *to;
	int sign, err;

	from = &cmd->args[0].id;
	to   = &cmd->args[1].id;

	if ((err = parseinput(start, &cmd->args[0], BAR_ARG))) return err;
	if (cmd->args[0].type == BAR_ARG) *from = START_PLC(game->turn);

	sign = (*end == '-' ? -1 : (*end == '+' ? 1 : 0)); /* if sign skip it */
	if (sign) end++;

	if ((err = parseinput(end, &cmd->args[1], HOME_ARG))) return err;
	if (sign) *to = *from + sign * (*to + 1);
	mvwprintw(game->root, 0, 0, "%d %d", *from , *to);
	wrefresh(game->root);
	
	return 0;
}


void
removecheckerat(Game *game, int pos)
{
	if (!--game->places[pos].count) { game->places[pos].pl = EMPTY; }
}

/*
 * ROLL 2 3 4 5  - roll and switch player (used when reading save)
 * MOVE BAR 23   - moves from place to place (basically like the prompt)
*/
int
executecmd(Game *game, Cmd *cmd)
{
	size_t idx;
	int from, to;

	switch (cmd->type) {
		case ROLL_CMD:
			game->turn = OTHER_PLAYER(game->turn);
			roll(game->dice);
			return 0;
		case MOVE_CMD:
			if (IS_EXE_ERR(idx = isvalidcmd(game, cmd))) return idx;
			from = FST_ARG(*cmd, game->turn);
			if (cmd->args[1].type == HOME_ARG) {
				game->home[game->turn]++;
				removecheckerat(game, from);
				return idx;
			}
			to = SND_ARG(*cmd, game->turn);
			if (game->places[to].pl == OTHER_PLAYER(game->turn)) { /* beating */
				game->bar[OTHER_PLAYER(game->turn)] += 1;
				game->places[to].count = 0;
			}
			game->places[to].pl = game->turn;
			game->places[to].count++;
			if (cmd->args[0].type == BAR_ARG) { game->bar[game->turn]--; }
			else { removecheckerat(game, from); }
			return idx;
		default: return CMD_NAME_ERR;
	}
}


int
main()
{
	Game *game;
	Cmd cmd;
	char start[INPUT_LEN], end[INPUT_LEN];
	size_t i; /* this is either index or error i love c error handling btw */

	setup();
	game = gamecreate();
	for (;;) {
		switch (game->state) {
		case START:
			gameinitdrw(game);
			do { roll(game->dice); } while (game->dice[0].val == game->dice[1].val);
			rolldrw(game, ME,  "[%d]", game->dice[ME].val);
			rolldrw(game, YOU, "[%d]", game->dice[YOU].val);
			game->turn = game->dice[ME].val > game->dice[YOU].val ? ME : YOU;
			game->dice[0].val = 1;
			game->dice[1].val = 2;
			game->state = RUN;
			goto READ;
		case RUN:
			roll(game->dice);
			rolldrw(game, game->turn, "[%d] [%d]", game->dice[0].val, game->dice[1].val);
		READ:
			/* readprompt -> parsecmdfromprompt -> executecmd */
			for (;;) {
				if (readprompt(game, start, end)) break;
				if (!IS_EXE_ERR(i = parsecmdfromprompt(&cmd, game, start, end)) &&
				    !IS_EXE_ERR(i = executecmd(game, &cmd))) {
						boarddrw(game);
						game->dice[i].use--;
						if (!game->dice[0].use && !game->dice[1].use) break;
						continue;
				}
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
