#include "View.hpp"
#include <ncurses.h>
#include <fstream>
#include <algorithm>
#include "Utils.hpp"

View::View(Model* _model, Signals* _signals) : model(_model), signals(_signals) {
	signals->sig_newFrame.connect(std::bind(&View::Render, this));
	signals->sig_quit.connect(std::bind(&View::Quit, this));
}

void View::Init() {
	// init curses
	initscr();
	cbreak();
	int h,w;
	m_window = newwin(LINES,COLS,0,0);
	keypad(m_window, true);
	m_window_size = glm::ivec2(COLS,LINES);
	curs_set(0); // hide cursor
	noecho();
	glm::ivec2 player_pos = model->GetPlayerPosition();
	m_camera_position = player_pos;
	m_game_window = m_window;
}

View::~View() {
	// deinit curses
	endwin();
}

std::ofstream fc("out2.log");
void View::GetInput() {
	int c = wgetch(m_window);
	fc << "ch " << c << "\n";
	signals->sig_input(c);
}

void View::Quit() {
	endwin();
	exit(0);
}

void View::drawRect(glm::ivec2 center, glm::ivec2 size) {
	int y1 = center.y - size.y/2;
	int y2 = center.y + size.y/2;
	int x1 = center.x - size.x/2;
	int x2 = center.x + size.x/2;
	mvwhline(m_window, y1, x1, 0, x2-x1);
	mvwhline(m_window, y2, x1, 0, x2-x1);
	mvwvline(m_window, y1, x1, 0, y2-y1);
	mvwvline(m_window, y1, x2, 0, y2-y1);
	mvwaddch(m_window, y1, x1, ACS_ULCORNER);
	mvwaddch(m_window, y2, x1, ACS_LLCORNER);
	mvwaddch(m_window, y1, x2, ACS_URCORNER);
	mvwaddch(m_window, y2, x2, ACS_LRCORNER);
}

// puts centered string with optional cursor
void View::putString(glm::ivec2 center, std::string str, int cursor) {
	mvwprintw(m_window, center.y, center.x - str.size()/2, str.c_str());
	if(cursor >= 0) {
		wattron( m_window, A_STANDOUT );
		mvwaddch(m_window, center.y, center.x - str.size()/2 + cursor, str[cursor]);
		wattroff( m_window, A_STANDOUT );
	}
}

void View::renderMenu() {
	wclear(m_window);
	
	const Menu* menu = model->GetMenu();
	if(!menu) return;
	
	// get max width of menu
	glm::ivec2 menu_size;
	menu_size.x = max_val(menu->items.begin(), menu->items.end(), [](const MenuItem &a) { return (int)a.name.size() + a.max_input; } );
	menu_size.y = menu->items.size() + 2;
	
	glm::ivec2 center = m_window_size / 2;
	
	int selected = model->GetSelection();
	int i = 0;
	
	drawRect(center, menu_size + glm::ivec2(10, 5));
	putString({center.x, 2}, menu->title);
	
	for(auto& m : menu->items) {
		int elem_size = m.name.size();
		glm::ivec2 pos( center.x, center.y - menu->items.size() + i );
		if(m.type == MenuItem::Type::inputfield) { // input box
			std::string input(m.max_input, '_');
			std::copy(m.input.begin(), m.input.end(), input.begin());
			elem_size += m.max_input;
			putString(pos, m.name + input, i == selected ? m.name.size() + m.input_cursor : -1);
		} else {
			putString(pos, m.name);
		}
		if(i == selected) {
			mvwaddch(m_window, pos.y, pos.x-2 - elem_size/2, '*');
			mvwaddch(m_window, pos.y, pos.x + elem_size/2 + 2, '*');
		}
		i++;
	}
}


static glm::ivec2 num_chunks(glm::ivec2 n, glm::ivec2 block) {
	return ((n % block) > 0) + n/block;
}


void View::renderGame() {
	auto player = model->GetPlayer();
	
	// game draw area
	glm::ivec2 draw_offset(0,2);
	glm::ivec2 draw_size = m_window_size - draw_offset;
	
	// move camera position if needed
	glm::ivec2 dist 	= glm::abs(player->position-m_camera_position) >= draw_size/2 * 3/4;
	m_camera_position 	= (1-dist) * m_camera_position + dist * (m_camera_position + glm::sign(player->position-m_camera_position));
	
	// clear window
	wclear(m_window);
	
	// draw status bar
	wmove(m_window, 0,0);
	if(true) {
		wprintw(m_window, "campos: %d %d ", player->position.x, player->position.y, m_camera_position.x, m_camera_position.y);
		wprintw(m_window, "wsize: %d %d ", m_window_size.x, m_window_size.y);
	}
	wprintw(m_window, "pos: %d %d hp: %d armor: %d", player->position.x, player->position.y, player->hp, player->armor);
	mvwhline(m_window, 1, 0, 0, m_window_size.x);
	//
	
	// make camera centered
	glm::ivec2 pos_offset   = m_camera_position - draw_size/2;
	glm::ivec2 chunk_size 	= glm::ivec2(Model::Chunk::xsize, Model::Chunk::ysize);
	glm::ivec2 chunk_offset = pos_offset / chunk_size;
	glm::ivec2 chunk_local  = pos_offset % chunk_size;
	
	// prepare drawing buffer in advance
	std::string xbuffer;
	xbuffer.resize(chunk_size.x);
	
	// number of chunks that can fit on window
	glm::ivec2 chunks_max = num_chunks(draw_size + chunk_local, chunk_size);
	
	// std::ofstream f("out.log");
	// f << "size: " << draw_size << " max: " << chunks_max << " loc: " << chunk_local << "\n";
	
	// render visible chunks
	for(int yc = 0; yc < chunks_max.y; yc++) {
		for(int xc = 0; xc < chunks_max.x; xc++) {
			
			glm::ivec2 chunk_pos(xc, yc);
			glm::ivec2 rel_pos = chunk_pos*chunk_size;
			auto &chunk = model->GetChunk(chunk_offset + chunk_pos);
			
			/*
					    |                    |
				[-------|-][--------][-------|-]
					    |                    |
				   offset_lt             offset_rb
			*/
			
			// left/top edge offset
			glm::ivec2 offset_lt = (chunk_pos == 0) * chunk_local;
			
			// right/bottom edge offset
			glm::ivec2 offset_rb = (chunk_pos != 0) * chunk_local;
			
			// characters remaining at right/bottom side
			glm::ivec2 length = glm::min(chunk_size, draw_size - (rel_pos - chunk_local)) - offset_lt;
			
			// render chunk
			for(int y = 0; y < length.y; y++) {
				for(int x = 0; x < length.x; x++) {
					glm::ivec2 pos = offset_lt + glm::ivec2(x,y);
					auto &obj 	= chunk.ObjectAt(pos);
					auto &place = xbuffer[x];
					// if(pos.x == 0) {place = 'A'; continue;}
					// find out what is located at this position and choose right character
					place = model->GetCharPalette()[(int)obj.type];
				}
				
				// put 8 characters at time
				glm::ivec2 put_pos = rel_pos - offset_rb + draw_offset + glm::ivec2(0,y);
				wmove(m_window, put_pos.y, put_pos.x);
				xbuffer[length.x] = '\0';
				// f << "put: " << chunk_pos << " " << glm::ivec2(0,y) << " " << xbuffer.c_str() << "\n";
				wprintw(m_window, xbuffer.c_str());
			}
		}
	}
}

void View::renderItemsMenu() {
	glm::ivec2 pos = m_window_size/2;
	glm::ivec2 size(10,5);
	// drawRect(
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
			renderGame();
			renderItemsMenu();
			break;
			
	}
	wrefresh(m_window);
}
