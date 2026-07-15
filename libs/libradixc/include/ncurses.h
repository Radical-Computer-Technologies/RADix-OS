#ifndef RADIX_SYSROOT_NCURSES_H
#define RADIX_SYSROOT_NCURSES_H

typedef unsigned long chtype;
#include <wchar.h>
typedef struct _radix_window WINDOW;

#define TRUE 1
#define FALSE 0
#define ERR (-1)
#define OK 0
#define KEY_CODE_YES 0400
#define KEY_UP 0403
#define KEY_DOWN 0402
#define KEY_LEFT 0404
#define KEY_RIGHT 0405
#define KEY_HOME 0406
#define KEY_BACKSPACE 0407
#define KEY_F0 0410
#define KEY_F(n) (KEY_F0 + (n))
#define KEY_ENTER 0527
#define KEY_END 0550
#define KEY_CANCEL 0543
#define KEY_DC 0512
#define KEY_IC 0513
#define KEY_SIC 0514
#define KEY_NPAGE 0522
#define KEY_PPAGE 0523
#define KEY_SR 0530
#define KEY_SF 0520
#define KEY_SLEFT 0531
#define KEY_SRIGHT 0532
#define KEY_A1 0533
#define KEY_A3 0534
#define KEY_B2 0535
#define KEY_C1 0536
#define KEY_C3 0537
#define KEY_SDC 0540
#define KEY_SCANCEL 0541
#define KEY_SUSPEND 0542
#define KEY_SSUSPEND 0544
#define KEY_BTAB 0545
#define KEY_BEG 0546
#define KEY_SBEG 0547
#define A_NORMAL 0
#define A_REVERSE 0x10000
#define A_BOLD 0x20000
#define COLOR_BLACK 0
#define COLOR_RED 1
#define COLOR_GREEN 2
#define COLOR_YELLOW 3
#define COLOR_BLUE 4
#define COLOR_MAGENTA 5
#define COLOR_CYAN 6
#define COLOR_WHITE 7

extern WINDOW *stdscr;
extern WINDOW *curscr;
extern int COLS;
extern int LINES;
extern int COLORS;

WINDOW *initscr(void);
WINDOW *newwin(int nlines, int ncols, int begin_y, int begin_x);
WINDOW *subwin(WINDOW *orig, int nlines, int ncols, int begin_y, int begin_x);
int delwin(WINDOW *win);
int endwin(void);
int isendwin(void);
int refresh(void);
int wrefresh(WINDOW *win);
int wnoutrefresh(WINDOW *win);
int doupdate(void);
int erase(void);
int clear(void);
int werase(WINDOW *win);
int wclear(WINDOW *win);
int move(int y, int x);
int wmove(WINDOW *win, int y, int x);
int addstr(const char *str);
int waddstr(WINDOW *win, const char *str);
int waddnstr(WINDOW *win, const char *str, int n);
int mvaddstr(int y, int x, const char *str);
int mvwaddstr(WINDOW *win, int y, int x, const char *str);
int mvwaddnstr(WINDOW *win, int y, int x, const char *str, int n);
int addch(const chtype ch);
int waddch(WINDOW *win, const chtype ch);
int mvwaddch(WINDOW *win, int y, int x, const chtype ch);
int printw(const char *format, ...);
int wprintw(WINDOW *win, const char *format, ...);
int mvwprintw(WINDOW *win, int y, int x, const char *format, ...);
int getch(void);
int ungetch(int ch);
int wgetch(WINDOW *win);
int get_wch(wint_t *wch);
int wget_wch(WINDOW *win, wint_t *wch);
int keypad(WINDOW *win, int bf);
int nodelay(WINDOW *win, int bf);
int noecho(void);
int echo(void);
int cbreak(void);
int nocbreak(void);
int halfdelay(int tenths);
int nonl(void);
int raw(void);
int noraw(void);
int curs_set(int visibility);
int wclrtoeol(WINDOW *win);
int clrtoeol(void);
int wattron(WINDOW *win, int attrs);
int wattroff(WINDOW *win, int attrs);
int wattrset(WINDOW *win, int attrs);
int attron(int attrs);
int attroff(int attrs);
int attrset(int attrs);
int start_color(void);
int has_colors(void);
int use_default_colors(void);
int init_pair(short pair, short f, short b);
int color_pair(short pair);
int set_escdelay(int ms);
int key_defined(const char *definition);
int define_key(const char *definition, int keycode);
int typeahead(int fd);
int wredrawln(WINDOW *win, int beg_line, int num_lines);
int scrollok(WINDOW *win, int bf);
int wscrl(WINDOW *win, int n);
int napms(int ms);
int beep(void);
#define COLOR_PAIR(n) (n)

#endif
