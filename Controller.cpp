#include "Controller.hpp"

#ifdef LINUX
#include <termios.h>
#endif
#include <ncurses.h>
#include <cctype>
#include <chrono>
#include <thread>
#include <random>

static Menu* main_menu;
static Menu* save_game;
static std::unique_ptr<Menu> items_menu;


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
			model->GenerateMap();
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
			{MenuItem::Type::button, "Quit Game", [=](){signals->sig_quit();}},
		}
	};

	
}

Controller::Controller(Model* _model, Signals* _signals) : model(_model), signals(_signals) {
	// must receieve keyboard input from view (as controller doesn't have reference to curses window, on purpose)
	signals->sig_input.connect(std::bind(&Controller::ProcessInput, this, std::placeholders::_1));

	InitMenus();
	model->SetMenu(main_menu);
	model->SetView(ViewType::menu);
	
	// render initial frame
	signals->sig_newFrame();
}

void Controller::Move(glm::ivec2 frompos, glm::ivec2 relpos) {
	glm::ivec2 oldPos = frompos;//model->GetPlayer()->position;
	auto &old_place = model->GetObjectAt(oldPos);
	if(!in(old_place.type, {Object::Type::enemy,Object::Type::friendly})) return;
	
	glm::ivec2 new_pos = oldPos + relpos;
	auto &place_to_go = model->GetObjectAt(new_pos);
	
	if(in(place_to_go.type, {Object::Type::empty, Object::Type::item})) {
		
		// what to move
		auto player = std::dynamic_pointer_cast<Actor>(old_place.obj);
		
		// pickup any items if there
		if(place_to_go.type == Object::Type::item) {
			player->items.push_back( std::dynamic_pointer_cast<ItemObject>(place_to_go.obj)->item );
		}
		
		// move to pos
		place_to_go.type = old_place.type;
		place_to_go.obj = old_place.obj;
		old_place.type = Object::Type::empty;
		old_place.obj.reset();
		player->position = new_pos;
		
	} else {
		
		// attack enemy
		if(in(place_to_go.type, {Object::Type::enemy, Object::Type::friendly}) && place_to_go.type != old_place.type) {
			auto target = std::dynamic_pointer_cast<Actor>(place_to_go.obj);
			auto player = std::dynamic_pointer_cast<Actor>(old_place.obj);
			model->SetAttackedPos(oldPos);
			player->hp -= target->damage;
			signals->sig_newFrame();
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
			
			model->SetAttackedPos(new_pos);
			target->hp -= player->damage;
			signals->sig_newFrame();
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
			
			if(target->hp <= 0) {
				place_to_go.type = Object::Type::empty;
				
				// drop some item if has any
				if(!target->items.empty()) {
					place_to_go.type = Object::Type::item;
					place_to_go.obj = std::make_unique<ItemObject>(target->items.front());
				}
				
				// remove from list
				model->GetEnemies().erase(target);
			}
			if(model->GetPlayer()->hp <= 0) {
				// you are dead dialog
				static Menu you_are_dead {
					"DEAD",
					{
						{MenuItem::Type::button, "<< Back to main menu", [=](){model->SetMenu(main_menu);}},
					}
				};
				model->SetMenu(&you_are_dead);
				model->SetView(ViewType::menu);
			}
			model->SetAttackedPos({-1,-1});
		}
	}
	
	signals->sig_newFrame();
}

void Controller::ToggleItemsDialog() {
	auto new_view = model->GetView() == ViewType::gamemenu ? ViewType::game : ViewType::gamemenu;
	auto player = model->GetPlayer();
	
	if(new_view == ViewType::gamemenu) {
		items_menu = std::unique_ptr<Menu>(new Menu({ "ITEMS" }));
		int i=0;
		for(auto& itm : player->items) {
			std::string name = std::string(itm.consumable ? "c" : "e") + " " + std::string(1,itm.charRepr);
			items_menu->items.push_back(MenuItem {
				MenuItem::Type::toggle, name, [=](){
					auto& item = player->items[i];
					item.equipped 	^= true; // toggle equip
					
					// equip item or unequip it
					player->hp 		+= (item.equipped ? 1 : -1) * item.hp;
					player->armor 	+= (item.equipped ? 1 : -1) * item.armor;
					player->damage 	+= (item.equipped ? 1 : -1) * item.damage;
					
					// remove item if consumable
					if(item.consumable) {
						item.charRepr = '\0';
						items_menu->items.erase(items_menu->items.begin() + model->GetSelection());
						model->IncrementSelection(-1); // move cursor back
					}
				},
				.input_cursor = itm.equipped
			});
			i++;
		}
		model->SetMenu( items_menu.get(), false );
	} else {
		// remove consumed items
		auto erased = std::remove_if(player->items.begin(), player->items.end(), [](const Item& itm) { return itm.charRepr == '\0'; });
		player->items.erase( erased, player->items.end() );
	}
	model->SetView( new_view );
	signals->sig_newFrame();
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
	
	if(in(model->GetView(), {ViewType::menu,ViewType::gamemenu})) {
		auto& item = model->GetMenu()->items[model->GetSelection()];
		
		if(item.type == MenuItem::Type::inputfield) {
			if(c == KEY_LEFT) { // move cursor left
				item.input_cursor = std::max(0, item.input_cursor-1);
			} else if(c == KEY_RIGHT) { // move cursor right
				item.input_cursor = std::min<int>(item.input.size(), item.input_cursor+1);
			} else if(c == KEY_BACKSPACE || c == '\b' || c == key_backspace) { // remove character with backspace
				item.input_cursor = std::max(0, item.input_cursor-1);
				item.input.erase(item.input_cursor, 1);
			}
			else {
				// insert character into inputfield
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
				if(item.type == MenuItem::Type::toggle) {
					item.input_cursor ^= 1;
					item.onclick();
				}
			}
		}
			
		signals->sig_newFrame();
	}
	
	// toggle items dialog
	if(c == 'i' && (model->GetView() == ViewType::game || model->GetMenu() == items_menu.get())) {
		ToggleItemsDialog();
	}
	
	// process arrow keys
	auto it = input_map.find(c);
	if(it != input_map.end()) {
		if(in(model->GetView(), {ViewType::menu,ViewType::gamemenu})) {
			if(!std::isalpha(c)) {
				model->IncrementSelection(it->second.y);
				signals->sig_newFrame();
			}
		} else {
			static std::default_random_engine re;

			std::uniform_int_distribution<int> unif(0,3);
			// move player
			Move(model->GetPlayer()->position, it->second);
			
			std::string keys = "wasd";
			for(auto &e : model->GetEnemies()) {
				Move(e->position, input_map[keys[unif(re)]]);
			}
		}
	}
}
