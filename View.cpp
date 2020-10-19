#include "View.hpp"
// #include <ncurses.h>

#include <fstream>
#include <algorithm>
#include "Utils.hpp"
#include "ViewColors.hpp"

View::View(Model* _model, Signals* _signals) : model(_model), signals(_signals) {
	signals->sig_new_frame.connect(std::bind(&View::Render, this));
	signals->sig_quit.connect(std::bind(&View::Quit, this));
}

void View::Init() {
	// init curses
	initscr();
	cbreak();
	int h,w;
	m_window = newwin(LINES,COLS,0,0);
	keypad(m_window, true);
	m_window_size = {0,0};
	curs_set(0); // hide cursor
	noecho();
	m_game_window = m_window;
	updateWindowSize();
	// init colors for water, trees and mountains
	start_color();
	init_colorpairs();
	
	// for left/top side panels
	m_lt_draw_offset = {0,2};
	// for right/bottom side panels
	m_rb_draw_offset = {0,0};
}

// std::ofstream fc("out2.log");
View::~View() {
	// deinit curses
	// fc.close();
	endwin();
}

void View::GetInput() {
	int c = wgetch(m_window);
	// fc << "ch " << c << "\n";
	signals->sig_input(c);
}

void View::Quit() {
	
	endwin();
	exit(0);
}

// draw border around dialogs
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

static glm::ivec2 num_chunks(glm::ivec2 n, glm::ivec2 block) {
	return ((n % block) > 0) + n/block;
}

// clear dialogs
void View::fillRect(glm::ivec2 center, glm::ivec2 size, char ch) {
	glm::ivec2 c = size/2;
	glm::ivec2 pos = center - c;
	std::string fill_with(size.x-1, ch);
	for(int y=1; y < c.y*2; ++y) {
		mvwprintw(m_window, pos.y+y, pos.x+1, fill_with.c_str());
	}
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
	
	const Menu* menu = model->GetMenu();
	if(!menu) return;
    
	// calculate menu size
	int max_menu = 10;
	glm::ivec2 menu_size;
	menu_size.x = std::max_element(menu->items.begin(), menu->items.end())->getWidth();
	menu_size.y = std::min<int>(max_menu, menu->items.size()) + 2;
	menu_size += glm::ivec2(10, 5);
	glm::ivec2 center = m_window_size / 2;
	
	// draw border, clear drawing area and put title
	fillRect(center, menu_size, ' ');
	drawRect(center, menu_size);
	putString({center.x, center.y-menu_size.y/2}, "[ " + menu->title + " ]");
	
	int selected = model->GetSelection();
	
	static int menu_position = 0;
	menu_position = std::max(0, selected - max_menu);
	selected -= menu_position;
	
	int i = 0;
	for(auto it = menu->items.begin()+menu_position; it != menu->items.end(); ++it) {
		auto &m = *it;
		int elem_size = m.name.size();
		glm::ivec2 pos( center.x, center.y - menu_size.y/2 + 2 + i );
		if(m.type == MenuItem::Type::inputfield) { // input box
			std::string input(m.max_input, '_');
			std::copy(m.input.begin(), m.input.end(), input.begin());
			elem_size += m.max_input;
			putString(pos, m.name + input, i == selected ? m.name.size() + m.input_cursor : -1);
		} else { // everything else
			if(m.input_cursor) {
				wattron(m_window, A_STANDOUT);
			}
			putString(pos, m.name);
			if(m.input_cursor) {
				wattroff(m_window, A_STANDOUT);
			}
		}
		if(i == selected) { // mark selection
			mvwaddch(m_window, pos.y, pos.x-2 - elem_size/2, '*');
			mvwaddch(m_window, pos.y, pos.x + elem_size/2 + 2, '*');
		}
		if(i > max_menu) break;
		i++;
	}
}

void View::updateWindowSize() {
	int mx,my;
	getmaxyx(stdscr, my, mx);
	glm::ivec2 new_win_size(mx,my);
	if(m_window_size != new_win_size) {
		m_lt_draw_offset = {0,2};
		m_rb_draw_offset = {0,0};
		signals->sig_canvas_size_changed(new_win_size - m_lt_draw_offset - m_rb_draw_offset);
		m_window_size = new_win_size;
	}
}

void View::renderGame() {
	auto player = model->GetPlayer();
	
	// game draw area (we can easily define drawing canvas)
	glm::ivec2 draw_size = model->GetCanvasSize();
	
	// clear window (commented out because it adds flickering)
	// wclear(m_window);
	
	// draw status bar
	mvwhline(m_window, 0, 0, ' ', m_window_size.x);
	wmove(m_window, 0,0);
	
	if(0) { // debug only
		glm::ivec2 campos = model->GetCameraPos();
		wprintw(m_window, "campos: %d %d ", player->position.x, player->position.y, campos.x, campos.y);
		wprintw(m_window, "wsize: %d %d ", m_window_size.x, m_window_size.y);
	}
	
	wprintw(m_window, "pos: %d %d | health: %d | armor: %d | damage: %d", player->position.x, player->position.y, player->hp, player->armor, player->damage);
	mvwhline(m_window, 1, 0, 0, m_window_size.x);
	//
	
	// make camera centered
	static glm::ivec2 chunk_size 	= glm::ivec2(Model::Chunk::xsize, Model::Chunk::ysize);
	glm::ivec2 pos_offset   = model->GetCameraPos() - draw_size/2;
	glm::ivec2 chunk_local  = pos_offset % chunk_size;
	glm::ivec2 chunk_offset = pos_offset / chunk_size;
	
	// handle negative positions
	chunk_offset -= (chunk_local < 0);
	chunk_local  = chunk_size * (chunk_local < 0) + chunk_local;
	
	// number of chunks that can fit on window
	glm::ivec2 chunks_max = num_chunks(draw_size + chunk_local, chunk_size);
	glm::ivec2 abs_player_pos = glm::abs(player->position); // for water animation
	// render visible chunks
	for(int yc = 0; yc < chunks_max.y; yc++) {
		for(int xc = 0; xc < chunks_max.x; xc++) {
			
			glm::ivec2 chunk_pos(xc, yc);
			glm::ivec2 relpos = chunk_pos*chunk_size;
			auto &chunk = model->GetChunk(chunk_offset + chunk_pos);
			
			/*
					    |                    |
				[-------|-][--------][-------|-]
					    |                    |
				   offset_lt             offset_rb
			*/
			
			// left/top edge offset
			glm::ivec2 offset_lt = (chunk_pos < 1) * chunk_local;
			
			// right/bottom edge offset
			glm::ivec2 offset_rb = (chunk_pos > 0) * chunk_local;
			
			// characters remaining at right/bottom side
			glm::ivec2 length = glm::min(chunk_size, draw_size - (relpos - chunk_local)) - offset_lt;
			
			// render chunk
			for(int y = 0; y < length.y; y++) {
				for(int x = 0; x < length.x; x++) {
					glm::ivec2 pos 	= offset_lt + glm::ivec2(x,y);
					auto &obj 		= chunk.TileAt(pos);
					
					glm::ivec2 color = {7,0};
					
					static int elevation_color_palette[] = {
						0b001,0b011,0b011,0b110,0b111
					};
					
					color.y = elevation_color_palette[obj.elevation+2];
					
					if(obj.type == Tile::tree) {
						color.x = 0b010;
						color.y = 0b010;
					}
					
					glm::ivec2 put_pos = relpos - offset_rb + m_lt_draw_offset + glm::ivec2(x,y);
					setcolor(m_window, color.x, color.y);
					if(obj.type == Tile::water) {
						// animated water
						static int water_anim = 0;
						water_anim = (x+y) & 1;
						mvwprintw(m_window, put_pos.y, put_pos.x, "%c", 
							(water_anim ^ (abs_player_pos.x^abs_player_pos.y)&1) ? ' ' : model->GetCharMap()[(int)obj.type]);
					} else {
						mvwprintw(m_window, put_pos.y, put_pos.x, "%c", 
							obj.type == Tile::empty ? model->GetElevationMap()[obj.elevation+2] : model->GetCharMap()[(int)obj.type]);
					}
					unsetcolor(m_window, color.x, color.y);
				}
			}
		}
	}
	
	// draw attack effect if in view
	auto atk_pos = model->GetAttackedPos();
	if( isInRect(atk_pos, pos_offset, draw_size) ) {
		auto &obj 	= model->GetTileAt(atk_pos);
		setcolor(m_window, 4,0);
		auto p = atk_pos - pos_offset + m_lt_draw_offset;
		mvwprintw(m_window, p.y, p.x, "%c", model->GetCharMap()[(int)obj.type]);
		unsetcolor(m_window, 4,0);
	}
}

void View::Render() {
	// get window size in case it changed
	updateWindowSize();
	
	switch(model->GetView()) {
		
		case ViewType::menu:
			wclear(m_window);
			renderMenu();
			break;
			
		case ViewType::game:
			renderGame();
			break;
			
		case ViewType::gamemenu:
			renderGame();
			renderMenu();
			break;
	}
	wrefresh(m_window);
}
