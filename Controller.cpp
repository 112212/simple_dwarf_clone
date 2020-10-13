#include "Controller.hpp"

#ifdef LINUX
#include <termios.h>
#endif
#include <ncurses.h>

Controller::Controller(Model* _model, View* view) : model(_model) {
	// must receieve keyboard input from view (as controller doesn't have reference to curses window, on purpose)
	view->sig_input.connect(std::bind(&Controller::ProcessInput, this, std::placeholders::_1));
	
	sig_newFrame.connect(std::bind(&View::Render, view));
	
	sig_quit.connect(std::bind(&View::Quit, view));
	
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
		// model->GetObjectAt(oldPos).swap(place_to_go);
		// place_to_go.swap(old_place);
		place_to_go.type = old_place.type;
		place_to_go.obj = old_place.obj;
		old_place.type = Object::Type::empty;
		old_place.obj.reset();
		model->GetPlayer()->position = new_pos;
	}
	
	sig_newFrame();
}


void Controller::ProcessInput(int c) {
	
	// std::cout << "read char: " << c << "\n";
	std::map<char, glm::ivec2> input_map {
		
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
	
	if(c == 'q') {
		sig_quit();
	}
	
	
	auto it = input_map.find(c);
	if(it != input_map.end()) {
		Move(it->second);
	}
}
