/* BACKGAMMON */
#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <unistd.h>
#include <ctype.h>
#include <locale.h> /* extend ASCII ncurses */
#include <ncursesw/curses.h>

#define PLAYER_COUNT     2
#define BEST_SCORES_NO   5
#define DICE_COUNT       2
#define NAME_MAX_LEN     24
#define TITLE            "The Game of GAMMON by yan3ku v0.1"
#define ROLL_PROMPT(p)   "%s roll> ", (p == ME ? "My" : "Your")
#define OTHER_PLAYER(p)  ((p) != EMPTY ? (p + 1) % 2 : EMPTY)
#define nelem(x)         (sizeof(x) / sizeof(*x))
#define TERM_SIZE_ERR    "Terminal window too small!"
#define PLAYER_SYM(pl)   ((pl) == ME ? "░░" : "▓▓")
#define PLAYER_DIR(pl)   ((pl) == ME ? "low to high" : "high to low")
#define SCORES_FILE      "scores"
#define UNUSED(p)        (void)(p)
#define INPUT_LEN        5
#define CMD_SIZ          4
#define ERR_DISP_TIME    2000
#define IS_HOME(pl, plc) ((pl) == ME ? 19 <= (plc) && (plc) <= BOARD_END : BOARD_BEG <= (plc) && (plc) <= 6)
#define IS_ERR(x)        ((x) >= DICE_COUNT)
#define BOARD_WIDTH      24
#define PLACES_SIZ       (BOARD_WIDTH + 2)
#define BOARD_BEG        1
#define BOARD_END        24
#define CHECKER_COUNT    15
#define MOVE_DIR(pl)     (pl ? -1 : +1);
#define START_PLC(pl)    ((pl) == ME ? BOARD_BEG - 1 : BOARD_END + 1)
#define BEAROFF_PLC(pl)  ((pl) == ME ? BOARD_END + 1 : BOARD_BEG - 1)
#define MAX(v1, v2)      ((v1) > (v2) ? v1 : v2)
#define HOME(game, pl)   ((game).places[BEAROFF_PLC(pl)].count)


typedef enum {
	ME,
	YOU,
	EMPTY,
} Player;

typedef enum {
	MENU,
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
	CONTINUE_GAME_ERR,
	END_TURN_ERR,
} ExeErr;

const char *ERR_STR[] = {
	[INDEX_UNBOUND_ERR]   = "No such place exist!",
	[INVALID_INPUT_ERR]   = "Wrong input! Usefull words: BAR, HOME, END, SAVE",
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
	char name[24];
	int points;
} Score;

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
	FILE *loaded;
	int steping;
	int skip;
	WINDOW *root;
	WINDOW *board;
	WINDOW *roll[PLAYER_COUNT];
	WINDOW *prompt;
	WINDOW *whome;
	int clearscores;
	/* +1 cause insert new score and sort the array */
	/* look 'loadscores' */
	Score scores[BEST_SCORES_NO + 1];
	int scores_no;
} Game;

/***************************************************
 * MISC
 ***************************************************/

#define notify(game, ...) do {                                       \
	curs_set(0);                                                     \
	wclear(game->prompt);                                            \
	mvwprintw(game->prompt, 0, 0, __VA_ARGS__);                      \
	wrefresh(game->prompt);                                          \
	napms(ERR_DISP_TIME);                                            \
	curs_set(1);                                                     \
} while (0)

#define enotify(game, ...) notify(game, "%s: %s", __VA_ARGS__, strerror(errno))

#define die(game, ...) {                                           \
	notify(game, __VA_ARGS__);                                     \
	endwin();                                                      \
	exit(1);                                                       \
}

#define edie(game, ...) die(game, "%s: %s", __VA_ARGS__, strerror(errno))

int /* newline consuming alternative to wget */
wread(WINDOW *win, char *input, int maxlen)
{
	int ch;
	char *beg = input;
	*input = '\0';
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

/************************************************************
 * INITIALIZATION
 ************************************************************/
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
scoresload(Game *game)
{
	FILE *score;
	char line[124];
	int i;
	if (!(score = fopen(SCORES_FILE, "r"))) {
		edie(game, "scores file");
	}

	for (i = 0; fgets(line, 124, score) && i < BEST_SCORES_NO; i++) {
		sscanf(line, "%s %d", game->scores[i].name, &game->scores[i].points);
	}
	game->scores_no = i;
}

void
gameinitsave(Game *game)
{
	if (!(game->save = tmpfile())) {
		wclear(game->prompt);
		mvwaddstr(game->prompt, 0, 0, strerror(errno));
		wrefresh(game->prompt);
		game->state = EXIT;
		return;
	}
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

	gameinitsave(game);
	scoresload(game);
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
	/*
	   game->places[23]  = (Place){.pl = ME,  .count = 1};
	   game->places[21]  = (Place){.pl = YOU,  .count = 1};
	   game->places[20]  = (Place){.pl = ME,  .count = 3};
	   game->places[19]  = (Place){.pl = YOU,  .count = 3};
	   game->places[18]  = (Place){.pl = YOU,  .count = 2};
	   game->places[22]  = (Place){.pl = ME,  .count = 1};
	*/

	game->state = MENU;
	game->clearscores = 1;
	return game;
}

/*******************************************************
 * DRAWING
 *******************************************************/
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
gamescoredrw(Game *game)
{
	char buff[64];
	sprintf(buff, "SCORE: Me %d, You %d", game->score[ME], game->score[YOU]);
	mvwaddstr(game->root, 0, ROOT_COLS - strlen(buff), buff);
}

void
bestscoresdrw(Game *game)
{
	int i, begx, begy;
	begy = (BOARD_LINES - game->scores_no) / 2;
	wclear(game->board);
	wborder(game->board, ':', ':', '=', '=', ':', ':', ':', ':');
	for (i = 0; i < game->scores_no; i++) {
		begx = (BOARD_COLS - strlen(game->scores[i].name)) / 2;
		mvwprintw(game->board, begy + i, begx, "%s, %d",
			 game->scores[i].name, game->scores[i].points);
	}
	wnoutrefresh(game->board);
}


void
boarddrwlines(Game *game)
{
	size_t i, j;
	int y;
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
}

void
boarddrwhome(Game *game)
{
	size_t i, j;
	int y;
	for (i = 0; i < 2; i++) { /* home */
		for (j = 0; j < CHECKER_COUNT; j++) {
			y = (i == YOU ? BOARD_LINES - 2 - j % 5 : j % 5 + 1);
			mvwaddstr(game->whome, y, j / 5 * 2, j < (size_t)HOME(*game, i) ? PLAYER_SYM(i) : "  ");
		}
	}

	mvwprintw(game->whome, BOARD_LINES / 2, 0, "[HOME]");
}

void
boarddrwplc(Game *game)
{
	size_t i, j;
	int x, y;
	for (i = BOARD_BEG; i <= BOARD_END; i++)  { /* places */
		for (j = 0; j < 6; j++) {
			plctoscr(i, j, &y, &x);
			mvwaddstr(game->board, y, x, plctosym(game->places, i, j));
		}
	}
}

void /* this function name means board draw (drw) btw */
boarddrw(Game *game)
{


	boarddrwlines(game);
	boarddrwhome(game);
	boarddrwplc(game);
	gamescoredrw(game);
	wnoutrefresh(game->whome);
	wnoutrefresh(game->board);
}

void
asdfasdjkf(Game *game)
{
	game->whome = derwin(game->root, BOARD_LINES, HOME_COLS, getpary(game->board), BOARD_COLS);
	mvwhline(game->whome, 0, 0, '=', HOME_COLS);
	mvwhline(game->whome, BOARD_LINES - 1, 0, '=', HOME_COLS);
	mvwvline(game->whome, 0, HOME_COLS - 1, ':', BOARD_LINES);
}

void
alsdkjfalksjdf(Game *game)
{
	game->root  = newwin(ROOT_LINES, ROOT_COLS,
	                     (LINES - ROOT_LINES) / 2,
	                     (COLS  - ROOT_COLS)  / 2);
	mvwaddstr(game->root, 0, 0, TITLE);

}

int
askdljfasldjf(Game *game)
{
	game->board = derwin(game->root, BOARD_LINES, BOARD_COLS, 3, 0);
	wborder(game->board, ':', ':', '=', '=', ':', ':', ':', ':');
	return getpary(game->board) + getmaxy(game->board);
}

int
gamemakewindows(Game *game)
{
	int boardendy;
	int x, y;

	alsdkjfalksjdf(game);
	boardendy = askdljfasldjf(game);
	plctoscr(14, 0, &y, &x); /* 14 places is at the left center */
	game->roll[0] = derwin(game->board, 1, 18, BOARD_LINES / 2, x);
	plctoscr(20, 0, &y, &x); /* 20 places is at the right center */
	game->roll[1] = derwin(game->board, 1, 18, BOARD_LINES / 2, x);

	game->prompt  = derwin(game->root, PROMPT_LINES, BOARD_COLS, boardendy + 2, 0);

	asdfasdjkf(game);
	return boardendy;
}


void
gamerefresh(Game *game)
{
	wnoutrefresh(stdscr);
	wnoutrefresh(game->root);
	wnoutrefresh(game->whome);
	wnoutrefresh(game->prompt);
	wnoutrefresh(game->board);
	doupdate();
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


	boardendy = gamemakewindows(game);
	boarddrw(game);
	bestscoresdrw(game);

	for (i = BOARD_BEG, j = 0; i <= BOARD_END; i++) {
		plctoscr(i, j, &y, &x);
		mvwprintw(game->root, i < 13 ? boardendy : getpary(game->board) - 1, x, "%2ld", i);
	}
	gamerefresh(game);

}

/****************************************************
 * MOVE VALIDATION
 ****************************************************/
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
	return count >= 15;
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

	return !((game->turn == ME  && from > to) ||
		 (game->turn == YOU && from < to));
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


/*****************************************
 * SAVE THINGS
 ****************************************/
/*
 * ROL 2 3 4 5   - roll and switch player
 * MOV BAR 23    - moves from place to place (basically like the prompt)
 * END           - explicitly end turn
 * TODO: Probably good idea to add anotations to ROL which player turn is it
 */
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
savewrite_roll(FILE *save, Dice dice[2])
{
	int i, j;
	fprintf(save, "ROL ");
	for (i = 0; i < DICE_COUNT; i++) {
		for (j = 0; j < dice[i].use; j++)
			fprintf(save, "%d ", dice[i].val);
	}
	fprintf(save, "\n");
}

void
savewrite_mov(FILE *save, MoveCmd *cmd)
{
	fprintf(save, "MOV %s ", plcidstr(cmd->from, BAR_WORD));
	fprintf(save, "%s\n", plcidstr(cmd->to, HOME_WORD));
}

void
savewrite_end(FILE *save)
{
	fprintf(save, "END\n");
}

int
readdice(Game *game)
{
	char ch;
	int i;
	char cmdname[CMD_SIZ];
	i = 0;
	fread(cmdname, CMD_SIZ - 1, 1, game->loaded);
	cmdname[CMD_SIZ - 1] = '\0';
	if (feof(game->loaded)) return CONTINUE_GAME_ERR;
	if (strcmp(cmdname, "ROL")) {
		die(game, "expected ROL command");
	}
	game->dice[0].use = game->dice[1].use = 1;
	while ((ch = fgetc(game->loaded)) != '\n' && ch != EOF) {
		if (isdigit(ch)) {
			ungetc(ch, game->loaded);
			fscanf(game->loaded, "%d", &game->dice[i++].val);
		}
		if (i == 2 && game->dice[0].val == game->dice[1].val) {
			i = 1;
			game->dice[0].use = 2;
		}
		if (i > 3) die(game, "to many argument to ROL");
	}
	return 0;
}

int
savewrite(Game *game)
{
	char buff[64];
	FILE *save;
	size_t size;

	wclear(game->prompt);
	mvwaddstr(game->prompt, 0, 0, "File path for save file: ");
	wgetnstr(game->prompt, buff, 64);
	if (!(save = fopen(buff, "w")))
		return errno;
	size = ftell(game->save);
	fseek(game->save, 0, SEEK_SET);
	if (sendfile(fileno(save), fileno(game->save), NULL, size) < 0)
		return errno;
	fclose(save);
	return 0;
}



void
printprompt(Game *game)
{
	int i, j;
	wclear(game->prompt);
	mvwprintw(game->prompt, 0, 0, "You %s have ", PLAYER_SYM(game->turn));
	for (i = 0; i < 2; i++)
		for (j = 0; j < game->dice[i].use; j++)
			wprintw(game->prompt, "[%d] ", game->dice[i].val);
	wprintw(game->prompt, "left, moving from %s", PLAYER_DIR(game->turn));
	mvwprintw(game->prompt, 1, 0, "Move from? ");
	wnoutrefresh(game->prompt);
}


int
readprompt(Game *game, char *start, char *end)
{
	*start = '\0';
	*end = '\0';

	for (;;) {
		printprompt(game);
		wread(game->prompt, start, INPUT_LEN);
		if (!strcmp(start, "END")) {
			mvwprintw(game->prompt, 1, 0, "End turn (ok/no)? ");
			wread(game->prompt, start, 3);
			if (!strcmp(start, "ok")) return END_TURN_ERR;
			continue;
		} else if (!strcmp(start, "SAVE")) {
			if (savewrite(game)) {
				enotify(game, "can't save game");
				continue;
			}
			notify(game, "Game saved!");
			continue;
		} else if (!strcmp(start, "EXIT")) {
			game->state = END;
			return END_TURN_ERR;
		} else break;
	};

	wprintw(game->prompt, " To? ");
	wread(game->prompt, end, INPUT_LEN);
	wclear(game->prompt);
	wnoutrefresh(game->prompt);
	return 0;
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
cmdparse(MoveCmd *move, const Game *game, char *start, char *end)
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

int
executecmd(Game *game, MoveCmd *cmd)
{
	size_t idx;

	if (IS_ERR(idx = isvalidcmd(game, cmd))) return idx;
	movechecker(game, cmd);
	savewrite_mov(game->save, cmd);
	return idx;
}


/*****************************************************************
 * GAME LOGIC AND STUFF IDK
 *****************************************************************/
void
roll(Game *game)
{
	int i;
	if (game->steping && readdice(game) != CONTINUE_GAME_ERR) {
		savewrite_roll(game->save, game->dice);
		return;
	}

	for (i = 0; i < 2; i++) game->dice[i].val = rand() % 6 + 1;
	game->dice[0].use = game->dice[1].use = (game->dice[0].val == game->dice[1].val ? 2 : 1);
	savewrite_roll(game->save, game->dice);
}

#define rolldrw(game, player, str, ...) do {				\
		mvwprintw(game->roll[player], 0, 0, ROLL_PROMPT(player)); \
		wprintw(  game->roll[player], str, __VA_ARGS__);	\
		wnoutrefresh( game->roll[player]);			\
	} while (0)

int
saveload(Game *game)
{
	char filename[128];
	wclear(game->prompt);
	mvwprintw(game->prompt, 0, 0, "Save path: ");
	wrefresh(game->prompt);
	curs_set(1);
	wgetnstr(game->prompt, filename, 128);
	curs_set(0);
	if (!(game->loaded = fopen(filename, "rw"))) {
		edie(game, "tmpfile");
		game->state = END;
		return errno;
	}
	game->steping = 1;
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
	if (select == 'L') saveload(game);
	game->state = START;
	curs_set(1);
}

void
gamestart(Game *game)
{
	if (game->clearscores) {
		game->clearscores = 0;
		wclear(game->board);
		wborder(game->board, ':', ':', '=', '=', ':', ':', ':', ':');
		boarddrw(game);
	}

	do { roll(game); } while (game->dice[0].val == game->dice[1].val);
	rolldrw(game, ME,  "[%d]", game->dice[ME].val);
	rolldrw(game, YOU, "[%d]", game->dice[YOU].val);
	game->turn = game->dice[ME].val > game->dice[YOU].val ? ME : YOU;
	game->state = PLAY;
	game->fstturn = 1;
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
	gamescoredrw(game);
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
skipblank(FILE *file) {
	char ch;
	while (isblank(ch = fgetc(file)) && ch != EOF);
}

int
promptstep(Game *game, char *cmdname, char *start, char *end)
{

	char ch;
	mvwprintw(game->prompt, 0, 0, "Turn of %s (C)ONTINUE / (P)AY / (S)SKIP...", PLAYER_SYM(game->turn));
	mvwprintw(game->prompt, 1, 0, "Next executing -- %s: %s %s", cmdname, start, end);
	switch ((ch = wgetch(game->prompt))) {
	case 'P': game->steping = 0; return CONTINUE_GAME_ERR;
	case 'S': game->skip = 1; break;
	}
	wrefresh(game->prompt);
	return 0;
}

int
readfile(Game *game, char *start, char *end)
{
	char cmdname[CMD_SIZ];
	int n;
	n = fread(cmdname, 1, CMD_SIZ - 1, game->loaded); /* todo handle eof */
	if (feof(game->loaded)) {
		game->steping = 0;
		return CONTINUE_GAME_ERR;
	}
	cmdname[n] = '\0';
	if (!strcmp(cmdname, "END")) {
		skipblank(game->loaded);
		return END_TURN_ERR;
	}
	if (strcmp(cmdname, "MOV")) {
		die(game, "expected MOV command, got: %s", cmdname);
	}
	fscanf(game->loaded, "%4s %4s", start, end);
	wclear(game->prompt);
	if (!game->skip)
		if (promptstep(game, cmdname, start, end)) return CONTINUE_GAME_ERR;
	skipblank(game->loaded);
	return 0;
}

int
cmdread(Game *game, MoveCmd *cmd)
{
	int err;
	char start[INPUT_LEN], end[INPUT_LEN];
	int (*reader)(Game *game, char *start, char *end);
	reader = game->steping ?  readfile : readprompt;
	if (IS_ERR(err = reader(game, start, end))) {
		if (err == END_TURN_ERR) savewrite_end(game->save);
		return err;
	}
	if (!IS_ERR(err = cmdparse(cmd, game, start, end))) return err;
	return 0;
}

void
gameroll(Game *game)
{
	if (!game->fstturn) {
		roll(game);
		rolldrw(game, game->turn, "[%d] [%d]", game->dice[0].val, game->dice[1].val);
	}
}

void
gameplay(Game *game)
{
	MoveCmd cmd;
	int err;
	size_t i;

	gameroll(game);
	for (;;) {
		if ((err = cmdread(game, &cmd)) == END_TURN_ERR) break;
		if (err == CONTINUE_GAME_ERR) continue;
		if (IS_ERR(err)) {
			errordrw(game, i);
			continue;
		}
		if (IS_ERR(i = executecmd(game, &cmd))) {
			errordrw(game, i);
			continue;
		}
		boarddrw(game);
		game->dice[i].use--;
		if (!game->dice[0].use && !game->dice[1].use) break;
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

int
scorecmp(const void *s1, const void *s2)
{
	return ((Score *)s2)->points - ((Score *)s1)->points;
}

void
savescores(Game *game)
{
	FILE *scores;
	int i;

	if (!(scores = fopen(SCORES_FILE, "w"))) {
		enotify(game, "can't open score file");
		return;
	}

	game->scores[game->scores_no].points = MAX(game->score[ME], game->score[YOU]);
	wclear(game->prompt);
	mvwaddstr(game->prompt, 0, 0, "Player name: ");
	wgetnstr(game->prompt, game->scores[game->scores_no].name, 24);
	qsort(game->scores, game->scores_no + 1, sizeof(Score), scorecmp);
	for (i = 0; i < (game->scores_no + 1) % 10; i++) {
		fprintf(scores, "%s %d\n", game->scores[i].name, game->scores[i].points);
	}
}


void
gamesavescores(Game *game)
{
	char buff[64];

	for (;;) {
		wclear(game->prompt);
		mvwaddstr(game->prompt, 0, 0, "Save scores (ok/no)? ");
		wgetnstr(game->prompt, buff, 2);
		if (!strcmp(buff, "ok")) {
			savescores(game);
			break;
		} else if (!strcmp(buff, "no")) {
			break;
		}
	}
}


void
gamesavestate(Game *game)
{
	char buff[64];

	for (;;) {
		wclear(game->prompt);
		mvwaddstr(game->prompt, 0, 0, "Save game (ok/no)? ");
		wgetnstr(game->prompt, buff, 2);
		if (!strcmp(buff, "ok")) {
			while (savewrite(game)) {
				enotify(game, "savewrite");
				continue;
			}
			break;
		} else if (!strcmp(buff, "no")) {
			break;
		}
	}
}

void
gameend(Game *game)
{
	gamesavestate(game);
	gamesavescores(game);
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
		case EXIT: break;		   /* silence */
		}
	}
	endwin();
	free(game);
	return 0;
}
