#include "View.hpp"
#include <ncurses.h>
#include <fstream>
#include <algorithm>

View::View(Model* _model) : model(_model) {
	
}

void View::Init() {
	// init curses
	initscr();
	cbreak();
	int h,w;
	m_window = newwin(LINES,COLS,0,0);
	keypad(m_window, true);
	m_window_size = glm::ivec2(COLS,LINES);
	
	glm::ivec2 player_pos = model->GetPlayerPosition();
	m_camera_position = player_pos;
}

View::~View() {
	// deinit curses
	endwin();
}

std::ofstream fc("out2.log");
void View::GetInput() {
	int c = wgetch(m_window);
	fc << "ch " << c << "\n";
	sig_input(c);
}

void View::Quit() {
	endwin();
	exit(0);
}

void View::drawRect(glm::ivec2 center, glm::ivec2 size) {
	glm::ivec2 top_left = center - size/2;
	
	// top part
	mvwaddch(m_window, top_left.y, top_left.x, ACS_ULCORNER);
	mvwaddch(m_window, top_left.y, top_left.x + size.x, ACS_URCORNER);
	mvwhline(m_window, top_left.y-1, top_left.x+1, ACS_HLINE, size.x - 2);
	
	// side part
	mvwvline(m_window, top_left.y, top_left.x, ACS_VLINE, size.y);
	mvwvline(m_window, top_left.y, top_left.x + size.x, ACS_VLINE, size.y);
	
	// bottom part
	mvwaddch(m_window, top_left.y + size.y, top_left.x, ACS_LLCORNER);
	mvwaddch(m_window, top_left.y + size.y, top_left.x + size.x, ACS_LRCORNER);
	mvwhline(m_window, top_left.y + size.y, top_left.x + 1, ACS_HLINE, size.x - 2);
}

void View::putString(glm::ivec2 center, std::string str) {
	mvwprintw(m_window, center.y, center.x - str.size()/2, str.c_str());
}

void View::renderMenu() {
	std::vector<std::string> menu {
		"New Game",
		"Save Game",
		"Load Game"
	};
	auto size_cmp = [](const std::string& a, const std::string &b) {return a.size() > b.size(); };
	glm::ivec2 menu_size = glm::ivec2(std::max_element(menu.begin(), menu.end(), size_cmp)->size(), menu.size());
	
	glm::ivec2 center = m_window_size / 2;
	// drawRect(center, {menu_size.x + 2, 1});
	int selected = 0;
	int i = 0;
	
	putString({center.x, 2}, "MAIN MENU");
	
	for(auto& m : menu) {
		glm::ivec2 pos( center.x - m.size()/2, center.y - menu_size.y + i );
		mvwprintw(m_window, pos.y, pos.x, "%s", m.c_str());
		if(i == selected) {
			mvwaddch(m_window, pos.y, pos.x-2, '*');
			mvwaddch(m_window, pos.y, pos.x + menu_size.x + 2, '*');
		}
		i++;
	}
	
}

void View::renderGame() {
	glm::ivec2 player_pos = model->GetPlayerPosition();
	
	// move camera position if needed
	glm::ivec2 dist = glm::abs(player_pos-m_camera_position) >= m_window_size/2 * 3/4;
	m_camera_position = (1-dist) * m_camera_position + 
		dist * (m_camera_position + glm::sign(player_pos-m_camera_position));
	
	// make camera centered
	glm::ivec2 pos_offset = m_camera_position - m_window_size/2;
	
	wclear(m_window);
	
	// draw status bar
	wmove(m_window, 0,0);
	wprintw(m_window, "pos: %d %d campos: %d %d", player_pos.x, player_pos.y, m_camera_position.x, m_camera_position.y);
	wprintw(m_window, " window size: %d %d", m_window_size.x, m_window_size.y);
	//
	
	int chunk_size = 8;
	glm::ivec2 chunk_offset = pos_offset / chunk_size;
	pos_offset = pos_offset % chunk_size;
	
	// number of chunks that can fit on window
	glm::ivec2 chunks_max = m_window_size/chunk_size + 1 + glm::min(glm::ivec2(1), pos_offset);
	wprintw(m_window, " yc xc: %d %d pos_offs: %d %d", chunks_max.x, chunks_max.y, pos_offset.x, pos_offset.y);
	
	
	// prepare buffer in advance
	std::string xbuffer;
	xbuffer.resize(m_window_size.x);
	
	glm::ivec2 render_offset(0,2);
	
	// render visible chunks
	for(int yc = 0; yc < chunks_max.y; yc++) {
		for(int xc = 0; xc < chunks_max.x; xc++) {
			
			glm::ivec2 chunk_pos(xc, yc);
			auto &chunk = model->GetChunk(chunk_offset + chunk_pos);
			glm::ivec2 rel_pos = chunk_pos*chunk_size;
			
			/*
					|                    |
				[---|-----][--------][---|-----]
					|                    |
				offset_lt               offset_rb
			*/
			
			// left/top edge offset
			glm::ivec2 offset_lt = (chunk_pos == 0) * pos_offset;
			
			// right/bottom edge offset
			glm::ivec2 offset_rb = (chunk_pos != 0) * pos_offset;
			
			// characters remaining at right/bottom side
			glm::ivec2 length = glm::min(glm::ivec2(chunk_size), m_window_size - (rel_pos - pos_offset)) - offset_lt;
			
			// render chunk
			for(int y = 0; y < length.y; y++) {
				for(int x = 0; x < length.x; x++) {
					glm::ivec2 pos(x,y);
					auto &obj = chunk.ObjectAt(pos + offset_lt);
					
					auto &place = xbuffer[pos.x];
					
					// find out what is located at this position and choose right character
					if(obj.type != Object::Type::empty) {
						place = 'O';
					} else {
						place = '-';
					}
				}
				
				// render 8 characters at time
				glm::ivec2 put_pos = rel_pos - offset_rb + glm::ivec2(0,y) + render_offset;
				wmove(m_window, put_pos.y, put_pos.x);
				xbuffer[length.x] = 0;
				wprintw(m_window, xbuffer.c_str());
			}
		}
	}
}

void View::renderItemsMenu() {
	
	
}

void View::Render() {
	// get window size in case it changed
	m_window_size = glm::ivec2(COLS,LINES);
	// std::cout << "win size: " << m_window_size << "\n";
	// wclear(m_window);
	
	switch(model->GetView()) {
		
		case ViewType::menu:
			renderMenu();
			break;
			
		case ViewType::game:
			renderGame();
			break;
			
		case ViewType::items:
			renderItemsMenu();
			break;
			
	}
	wrefresh(m_window);
}
