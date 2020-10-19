#pragma once
#include "Model.hpp"
#include "Signals.hpp"

#ifdef NCURSES
#include <ncurses.h>
#else
#include "libs/PDCurses/curses.h"
#endif

class View {
public:
	View(Model* model, Signals* signals);
	~View();
	void Init();
	void GetInput();
	void Render();
	void NewGame();
	void Quit();

private:

	void drawRect(glm::ivec2 center, glm::ivec2 size);
	void fillRect(glm::ivec2 center, glm::ivec2 size, char ch);
	void putString(glm::ivec2 center, std::string str, int cursor=-1);
	void renderMenu();
	void renderGame();
	void renderItemsMenu();
	void updateWindowSize();
	
	Signals* signals;
	Model* model;
	WINDOW* m_window;
	WINDOW* m_game_window;
	glm::ivec2 m_lt_draw_offset;
	glm::ivec2 m_rb_draw_offset;
	glm::ivec2 m_window_size;
	int m_menu_position;
};
