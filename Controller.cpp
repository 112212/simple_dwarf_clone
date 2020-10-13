#include "Controller.hpp"

#ifdef LINUX
#include <termios.h>
#endif
#include <ncurses.h>
#include <cctype>

static Menu* main_menu;
static Menu* save_game;

void Controller::InitMenus() {
	auto enter_menu = [=](Menu* menu) {
		Menu* _menu = menu;
		return [=]() {
			model->PushMenu(_menu);
		};
	};
	auto back = [=]() {
		model->PopMenu();
	};
	
	static Menu load_game {
		"LOAD GAME",
		{
			{MenuItem::Type::inputfield, "Enter save filename: ", .max_input = 15},
			{MenuItem::Type::button, "<< Back", back},
		}
	};
	
	static Menu new_game {
		"NEW GAME",
		{
			{MenuItem::Type::inputfield, "Enter game name: ", .max_input = 15},
			{MenuItem::Type::button, "<< Back", back},
		},
		[=]() {
			model->SetView(ViewType::game);
		}
	};

	save_game = new Menu{
		"SAVE GAME",
		{
			{MenuItem::Type::inputfield, "Enter save filename: "},
			{MenuItem::Type::button, "<< Back", back},
		}
	};
	
	main_menu = new Menu {
		"MAIN MENU",
		{
			{MenuItem::Type::button, "New Game", enter_menu(&new_game)},
			{MenuItem::Type::button, "Load Game", enter_menu(&load_game)},
			{MenuItem::Type::button, "Quit Game", [=](){sig_quit();}},
		}
	};

	
}

Controller::Controller(Model* _model, View* view) : model(_model) {
	// must receieve keyboard input from view (as controller doesn't have reference to curses window, on purpose)
	view->sig_input.connect(std::bind(&Controller::ProcessInput, this, std::placeholders::_1));
	
	sig_newFrame.connect(std::bind(&View::Render, view));
	
	sig_quit.connect(std::bind(&View::Quit, view));
	
	InitMenus();
	model->SetMenu(main_menu);
	model->SetView(ViewType::menu);
	
	// render initial frame
	sig_newFrame();
}

void Controller::Move(glm::ivec2 relpos) {
	glm::ivec2 oldPos = model->GetPlayer()->position;
	auto &old_place = model->GetObjectAt(oldPos);
	glm::ivec2 new_pos = oldPos + relpos;
	auto &place_to_go = model->GetObjectAt(new_pos);
	if(place_to_go.type != Object::Type::empty) {
		// we have obstacle or item?
	} else {
		// move to pos
		place_to_go.type = old_place.type;
		place_to_go.obj = old_place.obj;
		old_place.type = Object::Type::empty;
		old_place.obj.reset();
		model->GetPlayer()->position = new_pos;
	}
	
	sig_newFrame();
}


void Controller::ProcessInput(int c) {
	
	static std::map<char, glm::ivec2> input_map {
		// wasd keys
		{'a', {-1,0}},
		{'w', {0,-1}},
		{'s', {0,1} },
		{'d', {1,0} },
		
		// arrow keys
		{KEY_LEFT,  {-1,0}},
		{KEY_UP,  {0,-1}},
		{KEY_DOWN,  {0,1} },
		{KEY_RIGHT,  {1,0} },
	};
	
	const int key_backspace = 127;
	
	if(model->GetView() == ViewType::menu) {
		auto& item = model->GetMenu()->items[model->GetSelection()];
		
		if(item.type == MenuItem::Type::inputfield) {
			
			if(c == KEY_LEFT) {
				item.input_cursor = std::max(0, item.input_cursor-1);
			} else if(c == KEY_RIGHT) {
				item.input_cursor = std::min<int>(item.input.size(), item.input_cursor+1);
			} else if(c == KEY_BACKSPACE || c == '\b' || c == key_backspace) {
				item.input_cursor = std::max(0, item.input_cursor-1);
				item.input.erase(item.input_cursor, 1);
			}
			else {
				if(item.input.size() < item.max_input && (std::isalnum(c) || std::isspace(c)) && c != '\n') {
					item.input.insert(item.input_cursor, 1, c);
					item.input_cursor = std::min<int>(item.input.size(), item.input_cursor+1);
				}
			}
			
			if(c == '\n') {
				model->GetMenu()->onclick();
			}
			
		} else {
			if(c == KEY_BACKSPACE || c == '\b' || c == key_backspace) {
				model->PopMenu();
			} else if(c == KEY_ENTER || c == '\n') {
				if(item.type == MenuItem::Type::button) {
					item.onclick();
				}
			}
		}
			
		sig_newFrame();
	}
	
	// process arrow keys
	auto it = input_map.find(c);
	if(it != input_map.end()) {
		if(model->GetView() == ViewType::menu) {
			if(!std::isalpha(c)) {
				model->IncrementSelection(it->second.y);
				sig_newFrame();
			}
		} else {
			Move(it->second);
		}
	}
}
