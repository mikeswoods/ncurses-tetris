#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include <time.h>

#define TETRIS_START_DROP_TIME 300

#define TETRIS_SHAPE_LEFT   1
#define TETRIS_SHAPE_RIGHT  2
#define TETRIS_SHAPE_ROTATE 3
#define TETRIS_MAX_REPEAT   3

#define TETRIS_COLS 10
#define TETRIS_ROWS 20

#define TETRIS_SHAPE_ROWS 4
#define TETRIS_SHAPE_COLS 4

#define TETRIS_SHAPE_COUNT 7
#define TETRIS_SHAPE_I 0
#define TETRIS_SHAPE_J 1
#define TETRIS_SHAPE_L 2
#define TETRIS_SHAPE_O 3
#define TETRIS_SHAPE_S 4
#define TETRIS_SHAPE_T 5
#define TETRIS_SHAPE_Z 6

#define TETRIS_COLOR_RED     1
#define TETRIS_COLOR_WHITE   2
#define TETRIS_COLOR_MAGENTA 3
#define TETRIS_COLOR_BLUE    4
#define TETRIS_COLOR_GREEN   5
#define TETRIS_COLOR_YELLOW  6
#define TETRIS_COLOR_CYAN    7

#define SWAP(type, va, vb) do {type t=va; va=vb;vb=t;} while(0)

typedef int TETRIS_shape_type;
typedef int TETRIS_shape_color;

struct TETRIS_shape {
  TETRIS_shape_type type;
	TETRIS_shape_color color;
  short row;
	short col;
  short max_w;
	short max_h;
  short map[TETRIS_SHAPE_ROWS][TETRIS_SHAPE_COLS];
};

// Shape points defined top-left to bottom-right.
struct TETRIS_shape TETRIS_shapes_table[] = {
  { TETRIS_SHAPE_I, TETRIS_COLOR_RED,     0, 0, 4, 1, {{1,1,1,1}}},       // I
  { TETRIS_SHAPE_J, TETRIS_COLOR_WHITE,   0, 0, 3, 2, {{1,0,0},{1,1,1}}}, // J
  { TETRIS_SHAPE_L, TETRIS_COLOR_MAGENTA, 0, 0, 3, 2, {{0,0,1},{1,1,1}}}, // L
  { TETRIS_SHAPE_O, TETRIS_COLOR_BLUE,    0, 0, 2, 2, {{1,1},{1,1}}},     // O
  { TETRIS_SHAPE_S, TETRIS_COLOR_GREEN,   0, 0, 3, 2, {{0,1,1},{1,1,0}}}, // S
  { TETRIS_SHAPE_T, TETRIS_COLOR_YELLOW,  0, 0, 3, 2, {{0,1,0},{1,1,1}}}, // T
  { TETRIS_SHAPE_Z, TETRIS_COLOR_CYAN,    0, 0, 3, 2, {{1,1,0},{0,1,1}}}  // Z
};

/* >>>>>>>>>>>>>>>>>>>> GLOBALS >>>>>>>>>>>>>>>>>>>> */

static short** MAIN;
static short** MASK1;
static short** MASK2;
static unsigned int _DROP_TIME_ = TETRIS_START_DROP_TIME;
static unsigned int _DROP_FLAG_ = 0;
static unsigned int _SCORE_     = 0;
static unsigned int _LEVEL_     = 0;
static unsigned int _STREAK_    = 0;

/* >>>>>>>>>>>>>>>>>>>> BOARD >>>>>>>>>>>>>>>>>>>> */

short** TETRIS_board_create()
{
	short **board;
	int i;

	board = (short **)calloc(TETRIS_ROWS, sizeof(short*));
	for (i=0; i<TETRIS_ROWS; i++)
		board[i] = (short *)calloc(TETRIS_COLS, sizeof(short));

	return board;
}

void TETRIS_board_destroy(short** board)
{
	int i;

	for (i=0; i<TETRIS_COLS; i++)
		free(board[i]);
	free(board);
}

void TETRIS_board_zero(short** board)
{
	int r;

	for (r=0; r<TETRIS_ROWS; r++)
		bzero(board[r], sizeof(short)*TETRIS_COLS);
}

void TETRIS_board_set(short** board, struct TETRIS_shape* shape)
{
	int rb, rs, cb, cs;

	for (rb=shape->row, rs=0; rs<TETRIS_SHAPE_ROWS && rb<TETRIS_ROWS; rb++, rs++)
		for (cb=shape->col, cs=0; cs<TETRIS_SHAPE_COLS && cb<TETRIS_COLS; cb++, cs++)
			if (board[rb][cb] | shape->map[rs][cs] && board[rb][cb] == 0)
				board[rb][cb] = shape->color;
}

void TETRIS_board_draw(short** board, short** mask)
{
	int rt,rc,r,c,scr_rows,scr_cols,row_offset,col_offset;
	TETRIS_shape_color color;

	getmaxyx(stdscr, scr_rows, scr_cols);
	row_offset = 2;
	col_offset = (int)((scr_cols/2)-(TETRIS_COLS/2));

	for (r=0; r<TETRIS_ROWS; r++) {
		for (c=0; c<TETRIS_COLS; c++) {
			color = board[r][c] | mask[r][c];
			rt = r + row_offset;
			rc = c * 2 + col_offset;
			if (color != 0) {
				attron(COLOR_PAIR(color));
				mvaddch(rt, rc, ' ');
				mvaddch(rt, rc+1, ' ');
				attroff(COLOR_PAIR(color));
			} else {
				mvaddch(rt, rc, '_');
				mvaddch(rt, rc+1, '_');
			}
			//mvprintw(r+row_offset, c+40, "%d ", board[r][c]);
		}
		//mvprintw(r+row_offset, col_offset+TETRIS_COLS+1, "%.2d ", r);
	}
	// Move the cursor to (0,0) and disable it
	move(scr_rows-1, scr_cols-1);
	curs_set(0);
	refresh();
}

int TETRIS_board_get_top_row(short** board, int from_row)
{
	int i,j,top_row,complete;

	i = top_row = from_row;
	while (i >= 0) {
		complete = 1;
		for (j=0; j<TETRIS_COLS; j++) {
			if (board[i][j] != 0) {
				complete = 0;
				top_row--;
				i--;
				break;
			}
		}
		if (complete)
			return top_row + 1;
	}

	return top_row;
}

int TETRIS_board_count_full_rows(short** board, int top_row)
{
	int r,rr,c,full,count;
	int empty[TETRIS_ROWS];

	bzero(&empty, sizeof(empty));

	// Travel up, from TETRIS_ROWS - 1 to check_row.
	count = 0;
	for (r=TETRIS_ROWS - 1; r>=top_row; r--) {
		full = 1;
		for (c=0; c<TETRIS_COLS; c++) {
			if (board[r][c] == 0) {
				full = 0;
				break;
			}
		}
		if (full) {
			bzero(board[r], sizeof(short)*TETRIS_COLS);
			empty[r] = 1;
			count++;
		}

	}
	// Coalesce the rows now.
	r=TETRIS_ROWS - 1;
	while(r > top_row) {
		if (empty[r]) {
			rr = r;
			while (empty[rr] && rr > top_row)
				rr--;
			memcpy(board[r], board[rr], sizeof(short)*TETRIS_COLS);
			empty[r]  = 0;
			empty[rr] = 1;
		}
		r--;
	}

	return count;
}

int TETRIS_update_score(short** board, int top_row)
{
	int rem_rows,drop_bonus;

	rem_rows = TETRIS_board_count_full_rows(board, 0);
	drop_bonus = _DROP_FLAG_ ? 500 : 0;

	if (rem_rows > 0)
		_STREAK_++;

	switch (rem_rows) {
		case 4: _SCORE_ += (40   * (_LEVEL_+1) * _STREAK_) + drop_bonus; break;
		case 3: _SCORE_ += (100  * (_LEVEL_+1) * _STREAK_) + drop_bonus; break;
		case 2: _SCORE_ += (300  * (_LEVEL_+1) * _STREAK_) + drop_bonus; break;
		case 1: _SCORE_ += (1200 * (_LEVEL_+1) * _STREAK_) + drop_bonus; break;
		case 0: default: _STREAK_ = 0;
	}

	// Every 100 points, decrease the _DROP_TIME_ by 10 ms.
	if ((_SCORE_ % 1000) == 0)
		_DROP_TIME_ += 10;

	return rem_rows;
}

/* >>>>>>>>>>>>>>>>>>>> SHAPE >>>>>>>>>>>>>>>>>>>> */

struct TETRIS_shape* TETRIS_shape_copy(struct TETRIS_shape* copy, struct TETRIS_shape* shape)
{
	int i;

	copy->type  = shape->type;
	copy->color = shape->color;
	copy->row   = shape->row;
	copy->col   = shape->col;
	copy->max_w = shape->max_w;
	copy->max_h = shape->max_h;
	for (i=0; i<TETRIS_SHAPE_ROWS; i++)
		memcpy(&copy->map[i], &shape->map[i], sizeof(short)*TETRIS_SHAPE_COLS);

	return copy;
}

struct TETRIS_shape* TETRIS_shape_copy_random(struct TETRIS_shape* copy)
{
	int r = (int)(((float)TETRIS_SHAPE_COUNT) * (((float)rand()) / ((float)RAND_MAX)));
	return TETRIS_shape_copy(copy, &TETRIS_shapes_table[r]);
}

struct TETRIS_shape* TETRIS_shape_copy_def(struct TETRIS_shape* copy, TETRIS_shape_type type)
{
	if (type >= 0 && type < TETRIS_SHAPE_COUNT)
		return TETRIS_shape_copy(copy, &TETRIS_shapes_table[type]);

	return NULL;
}

struct TETRIS_shape* TETRIS_shape_rotate(struct TETRIS_shape* shape)
{
	// A[x][y] <- A[shape->max_w-y][x]
	int r, c;
	short T[TETRIS_SHAPE_ROWS][TETRIS_SHAPE_COLS], diff;

	for (r=0; r<TETRIS_SHAPE_ROWS; r++)
		for (c=0; c<TETRIS_SHAPE_COLS; c++)
			T[r][c] = shape->map[TETRIS_SHAPE_COLS-1-c][r];
	for (r=0; r<TETRIS_SHAPE_ROWS; r++)
		memcpy(shape->map[r], &T[r], sizeof(short)*TETRIS_SHAPE_COLS);

	// Left-aligns the shape with its TETRIS_SHAPE_ROWS*TETRIS_SHAPE_COLS bounding box.
	while (1) {
		if ( ! shape->map[0][0] && ! shape->map[1][0] && ! shape->map[2][0] && ! shape->map[3][0]) {
			for (r=0; r<TETRIS_SHAPE_ROWS; r++)
				for (c=0; c<TETRIS_SHAPE_COLS-1; c++)
					SWAP(short, shape->map[r][c], shape->map[r][c+1]);
		} else {
			break;
		}
	}

	// Swap max_w, max_h
	SWAP(short, shape->max_w, shape->max_h);

	// Readjust to fit the bounding box
	diff = TETRIS_COLS - (shape->col + shape->max_w);
	if (diff < 0)
		shape->col += diff;
	diff = TETRIS_ROWS - (shape->row + shape->max_h);
	if (diff < 0)
		shape->row += diff;

	return shape;
}

// Shape collision detection functions

int TETRIS_shape_test_collision(short** board
                               ,short **mask
                               ,struct TETRIS_shape* shape
                               ,short offset_row
                               ,short offset_col)
{
	struct TETRIS_shape copy;
	int r = 0, c = 0;

	TETRIS_shape_copy(&copy, shape);
	TETRIS_board_zero(mask);
	copy.row += offset_row;
	copy.col += offset_col;
	TETRIS_board_set(mask, &copy);

	for (r=copy.row; r<copy.row+TETRIS_SHAPE_ROWS && r<TETRIS_ROWS; r++)
		for (c=copy.col; c<copy.col+TETRIS_SHAPE_COLS && c<TETRIS_COLS; c++)
			if (board[r][c] && mask[r][c]) /* Must use logical AND here, because board[r][c] >= 1 */
				return 1;

	return 0;
}

int TETRIS_shape_down_OK(short** board, short **mask, struct TETRIS_shape* shape)
{
	return shape->row + shape->max_h < TETRIS_ROWS &&
         !TETRIS_shape_test_collision(board, mask, shape, 1, 0);
}

int TETRIS_shape_right_OK(short** board, short **mask, struct TETRIS_shape* shape)
{
	return shape->col < TETRIS_COLS - shape->max_w &&
         !TETRIS_shape_test_collision(board, mask, shape, 0, 1);
}

int TETRIS_shape_left_OK(short** board, short **mask, struct TETRIS_shape* shape)
{
	return shape->col > 0 &&
         !TETRIS_shape_test_collision(board, mask, shape, 0, -1);
}

int TETRIS_shape_rotate_OK(short** board, short **mask, struct TETRIS_shape* shape)
{
	struct TETRIS_shape copy;

	TETRIS_shape_copy(&copy, shape);
	TETRIS_shape_rotate(&copy);

	return ! TETRIS_shape_test_collision(board, mask, &copy, 0, 0);
}

/* >>>>>>>>>>>>>>>>>>>> MAIN >>>>>>>>>>>>>>>>>>>> */

void TETRIS_set_drop_flag()
{
	_DROP_FLAG_ = 1;
	timeout(5);
}
void TETRIS_clear_drop_flag()
{
	_DROP_FLAG_ = 0;
	timeout(_DROP_TIME_);
}

void TETRIS_init()
{
	initscr();
	clear();

	noecho();
	cbreak();
	keypad(stdscr, TRUE);

	start_color();

	init_pair(TETRIS_COLOR_RED, COLOR_RED, COLOR_RED);
	init_pair(TETRIS_COLOR_WHITE, COLOR_WHITE, COLOR_WHITE);
	init_pair(TETRIS_COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA);
	init_pair(TETRIS_COLOR_BLUE, COLOR_BLUE, COLOR_BLUE);
	init_pair(TETRIS_COLOR_GREEN, COLOR_GREEN, COLOR_GREEN);
	init_pair(TETRIS_COLOR_YELLOW, COLOR_YELLOW, COLOR_YELLOW);
	init_pair(TETRIS_COLOR_CYAN, COLOR_CYAN, COLOR_CYAN);

	MAIN  = TETRIS_board_create();
	MASK1 = TETRIS_board_create();
	MASK2 = TETRIS_board_create();
}

void TETRIS_start()
{
	srand(time(NULL));
	TETRIS_board_zero(MAIN);
	TETRIS_board_zero(MASK1);
	TETRIS_board_zero(MASK2);
	TETRIS_board_draw(MAIN, MASK1);
}

int TETRIS_ask_quit()
{
	int key = ERR;

	mvprintw(1, 0, "Game over: Restart (r) or quit (q)?\n");
	timeout(0);
	while(key != 'q' || key != 'r') {
		key = getch();
		switch (key) {
			case 'r': return 0;
			case 'q': return 1;
		}
	}
  return 0;
}

void TETRIS_wait_unpause()
{
	mvprintw(1, 0, "*** Game Paused. Press any key to resume. ***");
	timeout(0);
	while (getch() == ERR); // Block for key
}

void TETRIS_destroy()
{
	TETRIS_board_destroy(MAIN);
	TETRIS_board_destroy(MASK1);
	TETRIS_board_destroy(MASK2);
	endwin();
}

int main(int argc, char** argv)
{
	struct TETRIS_shape shape;
	int key,ticks,action_ticks,top_row,full_row,allow_drop,force_drop,skip_tick;

	TETRIS_init();

restart:
  TETRIS_start();

  key = ticks = skip_tick = action_ticks = force_drop = 0;
  full_row = top_row = TETRIS_ROWS - 1;

  // Set getch() to non-blocking mode. This also controls the "drop time"
  // of a tetromino.
  timeout(_DROP_TIME_);

next_piece:
  // Get initial piece
  TETRIS_shape_copy_random(&shape);
  shape.col = TETRIS_COLS / 2;

  while (top_row >= 0) {

    TETRIS_board_zero(MASK2);

    key = getch();
    allow_drop = 1;
    skip_tick = 0;

    if (key != ERR) {
      force_drop = ! (action_ticks % TETRIS_MAX_REPEAT);
      switch (key) {
        case KEY_UP:
        case 65: /* OSX 10.4 */
        case 'r':
        case 'x':
        case ' ':
          if (TETRIS_shape_rotate_OK(MAIN, MASK2, &shape))
            TETRIS_shape_rotate(&shape);
          allow_drop = 0;
          action_ticks++;
          if (force_drop)
            goto piece_down;
          else
            break;
        case KEY_LEFT:
        case 68: /* OSX 10.4 */
        case 'a':
          if (TETRIS_shape_left_OK(MAIN, MASK2, &shape))
            shape.col--;
          allow_drop = 0;
          action_ticks++;
          if (force_drop)
            goto piece_down;
          else
            break;
        case KEY_RIGHT:
        case 67: /* OSX 10.4 */
        case 's':
          if (TETRIS_shape_right_OK(MAIN, MASK2, &shape))
            shape.col++;
          allow_drop = 0;
          action_ticks++;
          if (force_drop)
            goto piece_down;
          else
            break;
        case KEY_DOWN:
        case 66: /* OSX 1.0 */
        case 'z':
          piece_down:
            if (allow_drop)
              TETRIS_set_drop_flag();
            if (TETRIS_shape_down_OK(MAIN, MASK2, &shape)) {
              shape.row++;
            } else {
              // Shape is done; add to MAIN
              TETRIS_board_set(MAIN, &shape);
              top_row = TETRIS_board_get_top_row(MAIN, top_row);
              TETRIS_update_score(MAIN, top_row);
              TETRIS_clear_drop_flag();
              goto next_piece;
            }
            break;
        case 'p':
          TETRIS_wait_unpause();
          skip_tick = 1;
          continue;
        case 'q':
          if (TETRIS_ask_quit()) {
            goto quit;
          } else {
            goto restart;
          }
          break;
      }

    // Invalid key / tick w/o keypress occurred.
    } else if (TETRIS_shape_down_OK(MAIN, MASK2, &shape)) {
      shape.row++;
    } else {
      // Shape is done; add to MAIN
      TETRIS_board_set(MAIN, &shape);
      top_row = TETRIS_board_get_top_row(MAIN, top_row);
      TETRIS_update_score(MAIN, top_row);
      TETRIS_clear_drop_flag();
      goto next_piece;
    }

    //mvprintw(1, 0, "Ticks: %d [%d]", ticks, action_ticks);
    mvprintw(2, 0, "Score: %d", _SCORE_);
    mvprintw(3, 0, "Streak: %d", _STREAK_);
    mvprintw(4, 0, "Level: %d", _LEVEL_);
    //mvprintw(5, 0, "{key=%d, top-row=%d; shape=(%d,%d)}", key, top_row, shape.row, shape.col);

    TETRIS_board_zero(MASK1);
    TETRIS_board_set(MASK1, &shape);
    TETRIS_board_draw(MAIN, MASK1);

    if (!skip_tick)
      ticks++;
  }

quit:
	TETRIS_destroy();

	return 0;
}
