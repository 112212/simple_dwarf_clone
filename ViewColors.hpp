#pragma once
#ifdef NCURSES
#include <ncurses.h>
#else
#include "libs/PDCurses/curses.h"
#endif
int is_bold(int fg);
void init_colorpairs(void);
short curs_color(int fg);
int colornum(int fg, int bg);
void setcolor(WINDOW* win, int fg, int bg);
void unsetcolor(WINDOW* win, int fg, int bg);
