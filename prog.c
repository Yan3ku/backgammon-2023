/* ANSI C GAMMON                               -*- c-default-style "linux" -*- */
#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <unistd.h>
#include <locale.h> /* extend ASCII ncurses */
#include <ncursesw/curses.h>

#define SAVE_FILE        "gmsave"
#define PLAYER_COUNT     2
#define DICE_COUNT       2
#define TITLE            "The Game of GAMMON by yan3ku v0.1"
#define ROLL_PROMPT(p)   "%s roll> ", (p == ME ? "My" : "Your")
#define OTHER_PLAYER(p)  ((p) == ME ? YOU : (p) == YOU ? ME : EMPTY)
#define nelem(x)         (sizeof(x) / sizeof(*x))
#define TERM_SIZE_ERR    "Terminal window too small!"
#define PLAYER_SYM(pl)   ((pl) == ME ? "░░" : "▓▓")
#define PLAYER_DIR(pl)   ((pl) == ME ? "low to high" : "high to low")
#define UNUSED(p)        (void)(p)
#define INPUT_LEN        5
#define CMD_SIZ          5
#define ERR_DISP_TIME    2000
#define IS_HOME(pl, plc) ((pl) == ME ? 19 <= (plc) && (plc) <= BOARD_END : BOARD_BEG <= (plc) && (plc) <= 6)
#define IS_ERR(x)        ((x) >= DICE_COUNT)
#define BOARD_WIDTH      24
#define PLACES_SIZ       (BOARD_WIDTH + 2)
#define BOARD_BEG        1
#define BOARD_END        24
#define CHECKER_COUNT    15
/* not used as indexes to game.places array but for calculations of distance */
#define MOVE_DIR(pl)     (pl ? -1 : +1);
#define START_PLC(pl)    ((pl) == ME ? BOARD_BEG - 1 : BOARD_END + 1)
#define BEAROFF_PLC(pl)  ((pl) == ME ? BOARD_END + 1 : BOARD_BEG - 1)
#define HOME(game, pl)   ((game).places[BEAROFF_PLC(pl)].count)


typedef enum {
	ME,
	YOU,
	EMPTY,
} Player;

typedef enum {
	MENU,
	STEP,
	START,
	PLAY,
	WON,
	END,
	EXIT,
} State;

typedef enum {
	INDEX_UNBOUND_ERR = DICE_COUNT, /* if function returns dice index it's not error */
	CMD_NAME_ERR,
	INVALID_INPUT_ERR,
	MOVE_DIR_ERR,
	IS_NOT_PLAYER_ERR,
	CANT_BEAT_ERR,
	NON_EMPTY_BAR_ERR,
	NO_CHECKERS_BAR_ERR,
	IS_NOT_HOME_ERR,
	NO_MOVE_ROLLED_ERR,
	CANT_BEAROFF_ERR,
	HAVE_TO_BEAT_ERR,
} ExeErr;

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
	[CANT_BEAROFF_ERR]    = "You can't bearoff right now!",
	[HAVE_TO_BEAT_ERR]    = "You have to beat checkers closest to bearoff!",
	[CMD_NAME_ERR]        = "No such command!",
};

enum { /* sizes used for spaces between piles */
	PILE_COLS      = 2,
	PILE_LINES     = 6,
	PILE_GAP_LINES = 3,
	PILE_GAP_COLS  = 3,
	BAR_COLS       = 3
};

enum { /* sizes used for drawing windows */
	BOARDER      = 2,
	PROMPT_LINES = 2,
	HOME_COLS    = 7,
	BOARD_LINES  = PILE_LINES * 2 + PILE_GAP_LINES + BOARDER,
	BOARD_COLS   = PILE_COLS * 12 + PILE_GAP_COLS * 14 + BAR_COLS + BOARDER,
	ROOT_LINES   = BOARD_LINES + PROMPT_LINES + 5,
	ROOT_COLS    = BOARD_COLS  + HOME_COLS,
};

typedef struct {
	int val;
	int use;
} Dice;

typedef enum {
	BAR_WORD,
	HOME_WORD,
	PLC_WORD,
} MoveKey;

typedef struct {
	MoveKey keyword; /* HOME, BAR or index to game.places array */
	int pos;
}  PlcId;

typedef struct {
	PlcId from, to;
} MoveCmd;

const char *KEYWORD2STR[] = {
	[BAR_WORD] = "BAR",
	[HOME_WORD] = "HOME",
};

/* the Cmd* represents turn execution in game */
typedef struct {
	enum {
		MOVE_CMD,
		ROLL_CMD,
	} type;
	union {
		MoveCmd move;
		int roll[4];
	} get;
} Cmd;

typedef struct {
	Player pl;
	int count;
} Place;

typedef struct {
	Place places[PLACES_SIZ]; /* BOARD_WIDTH + 2, the 2 edges are home places */
	int bar[PLAYER_COUNT];
	Player turn;
	State state;
	int score[PLAYER_COUNT];
	Dice dice[DICE_COUNT];
	int fstturn;
	FILE *save;
	WINDOW *root;
	WINDOW *board;
	WINDOW *roll[PLAYER_COUNT];
	WINDOW *prompt;
	WINDOW *whome;
} Game;

void
setup()
{
	srand(time(NULL));
	setlocale(LC_ALL, "");
	initscr();
	cbreak();
	start_color();
	keypad(stdscr, TRUE);
	use_default_colors();
}

void
initboard(Game *game)
{
	size_t i;

	for (i = 0; i < PLACES_SIZ; i++) {
		game->places[i].pl = EMPTY;
	}

	HOME(*game, ME) = HOME(*game, YOU) = 0;
	game->bar[ME] = game->bar[YOU] = 0;

	game->places[1]  = (Place){.pl = ME,  .count = 2};
	game->places[6]  = (Place){.pl = YOU, .count = 5};
	game->places[8]  = (Place){.pl = YOU, .count = 3};
	game->places[12] = (Place){.pl = ME,  .count = 5};

	for (i = BOARD_BEG; i < PLACES_SIZ / 2; i++) {
		game->places[PLACES_SIZ - i - 1].pl = OTHER_PLAYER(game->places[i].pl);
		game->places[PLACES_SIZ - i - 1].count = game->places[i].count;
	}
	wnoutrefresh(game->board);
	wnoutrefresh(game->whome);
	doupdate();
}

Game *
gamecreate()
{
	Game *game;

	game = calloc(1, sizeof *game);
	game->fstturn = 1;
	initboard(game);
	HOME(*game, ME) = 14;
	game->places[23]  = (Place){.pl = ME,  .count = 1};
	game->places[21]  = (Place){.pl = YOU,  .count = 1};
	game->places[20]  = (Place){.pl = ME,  .count = 3};
	game->places[19]  = (Place){.pl = YOU,  .count = 3};
	game->places[18]  = (Place){.pl = YOU,  .count = 2};
	game->places[22]  = (Place){.pl = ME,  .count = 1};


	/*
	*/


	game->state = MENU;
	return game;
}

/*
 * "Place to screen function"
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
	int col = abs((PLACES_SIZ / 2) - i - 1) - (i >= (PLACES_SIZ / 2));
	int bar = col < 6 ? 0 : BAR_COLS + PILE_GAP_COLS;
	*x = PILE_GAP_COLS + col * (PILE_GAP_COLS + PILE_COLS) + bar + 1;
	*y = (i < (PLACES_SIZ / 2) ? BOARD_LINES - j - 2 : j + 1);
}

const char * /* player to symbol (for drawing) */
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
scoredrw(Game *game)
{
	char buff[64];
	sprintf(buff, "SCORE: Me %d, You %d", game->score[ME], game->score[YOU]);
	mvwaddstr(game->root, 0, ROOT_COLS - strlen(buff), buff);
}

void /* this function name means board draw (drw) btw */
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
		for (j = 0; j < CHECKER_COUNT; j++) {
			y = (i == YOU ? BOARD_LINES - 2 - j % 5 : j % 5 + 1);
			mvwaddstr(game->whome, y, j / 5 * 2, j < (size_t)HOME(*game, i) ? PLAYER_SYM(i) : "  ");
		}
	}

	mvwprintw(game->whome, BOARD_LINES / 2, 0, "[HOME]");

	for (i = BOARD_BEG; i <= BOARD_END; i++)  { /* places */
		for (j = 0; j < 6; j++) {
			plctoscr(i, j, &y, &x);
			mvwaddstr(game->board, y, x, plctosym(game->places, i, j));
		}
	}
	scoredrw(game);

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
	mvwaddstr(game->root, 0, 0, TITLE);

	game->board = derwin(game->root, BOARD_LINES, BOARD_COLS, 3, 0);
	wborder(game->board, ':', ':', '=', '=', ':', ':', ':', ':');
	boardendy   = getpary(game->board) + getmaxy(game->board);

	plctoscr(14, 0, &y, &x); /* 14 places is at the left center */
	game->roll[0] = derwin(game->board, 1, 18, BOARD_LINES / 2, x);
	plctoscr(20, 0, &y, &x); /* 20 places is at the right center */
	game->roll[1] = derwin(game->board, 1, 18, BOARD_LINES / 2, x);

	game->prompt  = derwin(game->root, PROMPT_LINES, BOARD_COLS, boardendy + 2, 0);

	game->whome = derwin(game->root, BOARD_LINES, HOME_COLS, getpary(game->board), BOARD_COLS);
	mvwhline(game->whome, 0, 0, '=', HOME_COLS);
	mvwhline(game->whome, BOARD_LINES - 1, 0, '=', HOME_COLS);
	mvwvline(game->whome, 0, HOME_COLS - 1, ':', BOARD_LINES);

	boarddrw(game);

	for (i = BOARD_BEG, j = 0; i <= BOARD_END; i++) {
		plctoscr(i, j, &y, &x);
		mvwprintw(game->root, i < 13 ? boardendy : getpary(game->board) - 1, x, "%2ld", i);
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
	noecho();
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
	echo();
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
	for (count = 0, i = BOARD_BEG; i <= BOARD_END; i++) {
		if (IS_HOME(game->turn, i) && game->places[i].pl == game->turn) {
			count += game->places[i].count;
		}
	}
	count += HOME(*game, game->turn);
	return count > 15;
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
	dir = MOVE_DIR(game->turn);
	for (i = BEAROFF_PLC(game->turn) - 6 * dir; BOARD_BEG <= i && i <= BOARD_END; i += dir) {
		if (game->places[i].pl == game->turn) { return i == pos; }
	}
	return -1;
}

int /* this functions fails when you try to beat your own checkers, used by validmvcmd */
possiblebeating(const Game *game, int pos)
{
	if (game->places[pos].pl != OTHER_PLAYER(game->turn)) return 0;
	if (game->places[pos].count > 1) return 0;
	return 1;
}

int /* this function accepts beating your own checkers, used by findbeating */
cantbeat(const Game *game, int to)
{
	if (game->places[to].pl == OTHER_PLAYER(game->turn) && game->places[to].count > 1)
		return 1;
	return 0;
}

int
closest2bearoff(const Game *game, int old, int new)
{
	int dist_old, dist_new, bearoff;
	bearoff = BEAROFF_PLC(OTHER_PLAYER(game->turn));
	dist_old = abs(bearoff - old);
	dist_new = abs(bearoff - new);
	return dist_old < dist_new ? old : new;
}

int
findbeating(const Game *game, int from)
{
	int pos, dir, closest, i;

	closest = START_PLC(OTHER_PLAYER(game->turn));
	dir = MOVE_DIR(game->turn);
	for (i = 0; i < DICE_COUNT; i++) {
		if (game->dice[i].use == 0) continue;
		pos = from + game->dice[i].val * dir;
		if (pos < BOARD_BEG || BOARD_END < pos) continue;
		if (!possiblebeating(game, pos)) continue;
		closest = closest2bearoff(game, closest, pos);
	}

	return closest;
}

int
isforcedbeating(const Game *game)
{
	int i, closest;
	closest = START_PLC(OTHER_PLAYER(game->turn));

	wmove(game->root, 0, 0);
	if (game->bar[game->turn] > 0) {
		return findbeating(game, START_PLC(game->turn));
	}

	for (i = BOARD_BEG; i <= BOARD_END; i++) {
		if (game->places[i].pl == game->turn) {
			closest = closest2bearoff(game, closest, findbeating(game, i));
		}
	}
	return closest;
}

int
isvaliddir(const Game *game, int from , int to)
{

	if ((game->turn == ME  && from > to) ||
	    (game->turn == YOU && from < to))
		return 0;
	return 1;
}

int
havetobeat(const Game *game, int to)
{
	int pos;
	if ((pos = isforcedbeating(game)) != START_PLC(OTHER_PLAYER(game->turn)))
		if (pos != to) return 1;
	return 0;
}

int /* special cases for either bearoffing, moving from bar, or regular movement */
validatekeyword(const Game *game, MoveCmd *move)
{
	int from, to;
	from = move->from.pos;
	to   = move->to.pos;

	if (move->from.keyword == BAR_WORD) {
		if (game->bar[game->turn] == 0) return NO_CHECKERS_BAR_ERR;
		if (!IS_HOME(OTHER_PLAYER(game->turn), to)) return IS_NOT_HOME_ERR;
	} else if (move->to.keyword == HOME_WORD) {
		if (!canbearoff(game)) return CANT_BEAROFF_ERR;
		if (game->places[from].pl != game->turn) return IS_NOT_PLAYER_ERR;
	} else {
		if (game->bar[game->turn] != 0) return NON_EMPTY_BAR_ERR;
		if (from < BOARD_BEG || from > BOARD_END || to < BOARD_BEG || to > BOARD_END) return INDEX_UNBOUND_ERR;
		if (game->places[from].pl != game->turn) return IS_NOT_PLAYER_ERR;
	}
	return 0;
}

ExeErr
isvalidcmd(const Game *game, MoveCmd *move)
{
	int idx, err, from, to;

	from = move->from.pos;
	to   = move->to.pos;
	
	if (IS_ERR(err = validatekeyword(game, move))) return err;
	if (!isvaliddir(game, from, to)) return MOVE_DIR_ERR;
	if (havetobeat(game, to))        return HAVE_TO_BEAT_ERR;
	if (cantbeat(game, to))          return CANT_BEAT_ERR;

	if (!IS_ERR(err = findmove(game->dice, abs(from - to), eq))) return err;
	if (move->to.keyword != HOME_WORD) return NO_MOVE_ROLLED_ERR;
	/*
	 * there is special rule that says:
	 * if the roll is greater than the distance from last checker to home you can bearoff it
	 */
	if (islastchecker(game, from) && !IS_ERR(idx = findmove(game->dice, abs(from - to), geq)))
			return idx;
	return NO_MOVE_ROLLED_ERR;
}

int
parseinput(char *input, PlcId *plc, MoveKey keyword)
{
	char *endptr;

	plc->pos = strtol(input, &endptr, 10);
	plc->keyword = PLC_WORD;
	if (input == endptr) {
		if ((!strcmp(input, KEYWORD2STR[keyword]))) { plc->keyword = keyword; }
		else { return INVALID_INPUT_ERR; }
	} else if (*endptr != '\0') { return INVALID_INPUT_ERR; }
	return 0;
}

int
parsecmdfromprompt(MoveCmd *move, const Game *game, char *start, char *end)
{
	int *from, *to;
	int sign, err;

	from = &move->from.pos;
	to   = &move->to.pos;

	if ((err = parseinput(start, &move->from, BAR_WORD))) return err;
	if (move->from.keyword == BAR_WORD) *from = START_PLC(game->turn);

	sign = (*end == '-' ? -1 : (*end == '+' ? 1 : 0)); /* if sign skip it */
	if (sign) end++;

	if ((err = parseinput(end, &move->to, HOME_WORD))) return err;
	if (move->to.keyword == HOME_WORD) *to = BEAROFF_PLC(game->turn);
	if (sign) *to = *from + sign * *to;
	wrefresh(game->root);

	return 0;
}


void
movechecker(Game *game, MoveCmd *move)
{
	int from , to;
	from = move->from.pos;
	to = move->to.pos;
	/* beat checker and move them to bar */
	if (game->places[to].pl == OTHER_PLAYER(game->turn)) {
		game->bar[OTHER_PLAYER(game->turn)] += 1;
		game->places[to].count = 0;
	}
	/* move it */
	game->places[to].pl = game->turn;
	game->places[to].count++;
	/* remove from old position */
	if (move->from.keyword == BAR_WORD) { game->bar[game->turn]--; }
	else if (!--game->places[from].count) { game->places[from].pl = EMPTY; }
}

/*
 * ROLL 2 3 4 5  - roll and switch player (used when reading save)
 * MOVE BAR 23   - moves from place to place (basically like the prompt)
 * TODO: Probably good idea to add anotations to ROLL which player turn is it
*/
int
executecmd(Game *game, Cmd *cmd)
{
	size_t idx;

	switch (cmd->type) {
		case ROLL_CMD:
			game->turn = OTHER_PLAYER(game->turn);
			roll(game->dice);
			return 0;
		case MOVE_CMD:
			if (IS_ERR(idx = isvalidcmd(game, &cmd->get.move))) return idx;
			movechecker(game, &cmd->get.move);
			return idx;
		default: return CMD_NAME_ERR;
	}
}

const char *
plcidstr(PlcId plc, MoveKey keyword)
{
	static char buff[4];

	if (plc.keyword == keyword) {
		return KEYWORD2STR[keyword];
	} else {
		sprintf(buff, "%d", plc.pos);
		return buff;
	}
}

void
savemvcmd(FILE *file, MoveCmd *cmd)
{
	fprintf(file, "MOVE %s %s\n", plcidstr(cmd->from, BAR_WORD), plcidstr(cmd->from, HOME_WORD));
}

void
savedice(FILE *file, Dice dice[2])
{
	int i, j;
	fprintf(file, "ROLL ");
	for (i = 0; i < DICE_COUNT; i++) {
		for (j = 0; j < dice[i].use; j++)
			fprintf(file, "%d ", dice[i].val);
	}
	fprintf(file, "\n");
}

int
errexit(Game *game, const char *msg)
{
	curs_set(0);
	wclear(game->prompt);
	mvwprintw(game->prompt, 0, 0, "%s: %s", msg, strerror(errno));
	wrefresh(game->prompt);
	napms(ERR_DISP_TIME);
	endwin();
	exit(1);
}

int
opensave(Game *game)
{
	char filename[128];
	wclear(game->prompt);
	mvwprintw(game->prompt, 0, 0, "Save path: ");
	wrefresh(game->prompt);
	curs_set(1);
	wgetnstr(game->prompt, filename, 128);
	curs_set(0);
	if (!fopen(filename, "r")) {
		errexit(game, "tmpfile");
		game->state = END;
		return errno;
	}
	game->state = END;
	return 0;
}


void
gamemenu(Game *game)
{
	char select;
	gameinitdrw(game);
	curs_set(0);
	do {
		mvwprintw(game->prompt, 0, 0, "(L)OAD/(P)LAY");
		wrefresh(game->prompt);
		select = getch();
	} while (select != 'P' && select != 'L');
	switch (select) {
		case 'L': opensave(game); break;
		case 'P': game->state = START; break;
	}
	curs_set(1);
}

void
gamestart(Game *game)
{
	if (!game->save) {
		game->save = tmpfile();
		if (!game->save) {
			wclear(game->prompt);
			mvwaddstr(game->prompt, 0, 0, strerror(errno));
			wrefresh(game->prompt);
			game->state = EXIT;
			return;
		}
	}
	do { roll(game->dice); } while (game->dice[0].val == game->dice[1].val);
	rolldrw(game, ME,  "[%d]", game->dice[ME].val);
	rolldrw(game, YOU, "[%d]", game->dice[YOU].val);
	game->turn = game->dice[ME].val > game->dice[YOU].val ? ME : YOU;
	game->dice[0].val = 1;
	game->dice[1].val = 2;
	game->state = PLAY;
}

void
errordrw(Game *game, int err)
{
	curs_set(0);
	wclear(game->prompt);
	mvwaddstr(game->prompt, 0, 0, ERR_STR[err]);
	wrefresh(game->prompt);
	napms(ERR_DISP_TIME);
	curs_set(1);
	wclear(game->prompt);
}

int
playerwon(Game *game)
{
	return HOME(*game, game->turn) == CHECKER_COUNT;
}

void
updatescore(Game *game)
{
	int *score = &game->score[game->turn];
	if (game->bar[OTHER_PLAYER(game->turn)]) {
		*score += 3;
	} else if (HOME(*game, OTHER_PLAYER(game->turn))) {
		*score += 2;
	} else *score += 1;
	scoredrw(game);
	wnoutrefresh(game->root);
}

void
gamewon(Game *game)
{
	char buff[4];
	updatescore(game);
	wclear(game->prompt);
	for (;;) {
		wclear(game->prompt);
		mvwprintw(game->prompt, 0, 0, "Player %s won!", PLAYER_SYM(game->turn));
		waddstr(game->prompt, " Continue (ok/end)? ");
		wgetnstr(game->prompt, buff, 3);
		if (!strcmp(buff, "ok")) {
			game->state = START;
			break;
		} else if (!strcmp(buff, "end")) {
			game->state = END;
			break;
		}
	}
	wrefresh(game->prompt);
	initboard(game);
	boarddrw(game);
}

void
gameplay(Game *game)
{
	Cmd cmd;
	char start[INPUT_LEN], end[INPUT_LEN];
	size_t i; /* this is either index or error i love c error handling btw */

	if (!game->fstturn) {
		roll(game->dice);
		rolldrw(game, game->turn, "[%d] [%d]", game->dice[0].val, game->dice[1].val);
	}
	savedice(game->save, game->dice);

	for (;;) {
		cmd.type = MOVE_CMD;
		if (readprompt(game, start, end)) break;
		if (!IS_ERR(i = parsecmdfromprompt(&cmd.get.move, game, start, end)) &&
				!IS_ERR(i = executecmd(game, &cmd))) {
			savemvcmd(game->save, &cmd.get.move);
			boarddrw(game);
			game->dice[i].use--;
			if (!game->dice[0].use && !game->dice[1].use) break;
			continue;
		}
		errordrw(game, i);
	}

	if (playerwon(game)) {
		game->state = WON;
		return;
	}
	wclear(game->roll[game->turn]);
	wnoutrefresh(game->roll[game->turn]);
	game->turn = OTHER_PLAYER(game->turn);
	game->fstturn = 0;
}

void
writesave(Game *game)
{
	char buff[64];
	FILE *save;
	size_t size;

	mvwaddstr(game->prompt, 0, 0, "File path for save file: ");
	wgetnstr(game->prompt, buff, 64);
	mvwprintw(game->root, 0, 0, "%s", buff);
	wrefresh(game->root);
	if (!(save = fopen(buff, "w"))) {
		errexit(game, "open");
	}
	size = ftell(game->save);
	fseek(game->save, 0, SEEK_SET);
	if (sendfile(fileno(save), fileno(game->save), NULL, size) < 0)
	   errexit(game, "sendfile");
	fclose(save);
}

void
gameend(Game *game)
{
	char buff[64];

	for (;;) {
		wclear(game->prompt);
		mvwaddstr(game->prompt, 0, 0, "Save game (ok/no)? ");
		wgetnstr(game->prompt, buff, 2);
		if (!strcmp(buff, "ok")) {
			writesave(game);
			break;
		} else if (!strcmp(buff, "no")) {
			break;
		}
	}
	game->state = EXIT;
}

int
main()
{
	Game *game;

	setup();
	game = gamecreate();
	while (game->state != EXIT) {
		switch (game->state) {
		case MENU:  gamemenu(game); break;
		case START: gamestart(game); break;
		case PLAY:  gameplay(game); break;
		case WON:   gamewon(game); break;
		case END:   gameend(game); break;
		case EXIT:  break;
		case STEP:  break;
		}
	}
	endwin();
	free(game);
	return 0;
}

/* vim: set ts=4 sts=4 sw=4 noet: */
