#include "Controller.hpp"

#ifdef LINUX
#include <termios.h>
#endif
#include <ncurses.h>
#include <cctype>
#include <chrono>
#include <thread>
#include <random>

#include "boost/filesystem/operations.hpp"

#include <fstream>

static std::unique_ptr<Menu> main_menu;
static std::unique_ptr<Menu> main_menu2;
static std::unique_ptr<Menu> save_game;
static std::unique_ptr<Menu> load_game;
static std::unique_ptr<Menu> items_menu;
static std::unique_ptr<Menu> new_game;


void Controller::InitMainMenu() {
	auto enter_menu = [=](Menu* menu) {
		Menu* _menu = menu;
		return [=]() {
			model->PushMenu(_menu);
		};
	};
	auto back = [=]() {
		model->PopMenu();
	};
	
	
	load_game = std::unique_ptr<Menu>(new Menu {
		"LOAD GAME",
		{}
	});
	UpdateLoadGameMenu();
	
	
	// menu while not running game
	
	new_game = std::unique_ptr<Menu>(new Menu {
		"NEW GAME",
		{
			{MenuItem::Type::inputfield, "Enter game name: ", .max_input = 15},
			{MenuItem::Type::button, "<< Back", back},
		},
		[=]() {
			model->GenerateMap();
			model->NewGame();
			model->SetView(ViewType::game);
			signals->sig_new_game();
		}
	});
	
	main_menu = std::unique_ptr<Menu>(new Menu {
		"MAIN MENU",
		{
			{MenuItem::Type::button, "New Game", enter_menu(new_game.get())},
			{MenuItem::Type::button, "Load Game", enter_menu(load_game.get())},
			{MenuItem::Type::button, "Quit Game", [=](){signals->sig_quit();}},
		}
	});
	
	// ESC menu while in game
	save_game = std::unique_ptr<Menu>(new Menu{
		"SAVE GAME",
		{
			{MenuItem::Type::inputfield, "Enter save filename: ", .max_input=15},
			{MenuItem::Type::button, "<< Back", back},
		},
		[=](){
			std::string savefile = model->GetMenu()->items[model->GetSelection()].input;
			model->SaveGame("savegames/"+savefile+".json");
			model->SetView(ViewType::game);
		}
	});
	
	main_menu2 = std::unique_ptr<Menu>(new Menu {
		"MAIN MENU",
		{
			{MenuItem::Type::button, "<< Back", [=](){model->SetView(ViewType::game);}},
			{MenuItem::Type::button, "New Game", enter_menu(new_game.get())},
			{MenuItem::Type::button, "Save Game", enter_menu(save_game.get())},
			{MenuItem::Type::button, "Load Game", enter_menu(load_game.get())},
			{MenuItem::Type::button, "Quit Game", [=](){signals->sig_quit();}},
		}
	});
	
}

Controller::Controller(Model* _model, Signals* _signals) : model(_model), signals(_signals) {
	// must receieve keyboard input from view (as controller doesn't have reference to curses window, on purpose)
	signals->sig_input.connect(std::bind(&Controller::ProcessInput, this, std::placeholders::_1));

	InitMainMenu();
	model->SetMenu(main_menu.get());
	model->SetView(ViewType::menu);
	
	// render initial frame
	signals->sig_new_frame();
}

void Controller::Move(glm::ivec2 frompos, glm::ivec2 relpos) {
	glm::ivec2 old_pos = frompos;
	auto &old_place = model->GetObjectAt(old_pos);
	if(!in(old_place.type, {Object::Type::enemy,Object::Type::friendly})) return;
	
	glm::ivec2 new_pos = old_pos + relpos;
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
			model->SetAttackedPos(old_pos);
			player->hp -= target->damage;
			signals->sig_new_frame();
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
			
			model->SetAttackedPos(new_pos);
			target->hp -= player->damage;
			signals->sig_new_frame();
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
						{MenuItem::Type::button, "<< Back to main menu", [=](){model->SetMenu(main_menu.get());}},
					}
				};
				model->SetMenu(&you_are_dead);
				model->SetView(ViewType::menu);
			}
			model->SetAttackedPos({-1,-1});
		}
	}
	
	signals->sig_new_frame();
}

void Controller::ToggleItemsDialog() {
	auto player = model->GetPlayer();
	auto back = [=](){
		// remove consumed items
		auto erased = std::remove_if(player->items.begin(), player->items.end(), [](const Item& itm) { return itm.idx == -1; });
		player->items.erase( erased, player->items.end() );
	};
	if(model->GetView() == ViewType::game) {
		items_menu = std::unique_ptr<Menu>(new Menu({ "ITEMS", .onback = back }));
		int i=0;
		for(auto& itm : player->items) {
			auto itm_def = model->GetItemDef(itm.idx);
			std::string name = std::string(itm_def.consumable ? "c" : "e") + " " + std::string(1,itm_def.charRepr);
			items_menu->items.push_back(MenuItem {
				MenuItem::Type::toggle, name, [=](){
					auto& item = player->items[i];
					auto& item_def = model->GetItemDef(item.idx);
					item.equipped 	^= true; // toggle equip
					
					// equip item or unequip it
					player->hp 		+= (item.equipped ? 1 : -1) * item_def.hp;
					player->armor 	+= (item.equipped ? 1 : -1) * item_def.armor;
					player->damage 	+= (item.equipped ? 1 : -1) * item_def.damage;
					
					// remove item if consumable
					if(item_def.consumable) {
						item.idx = -1;
						items_menu->items.erase(items_menu->items.begin() + model->GetSelection());
						model->IncrementSelection(-1); // move cursor back
					}
				},
				.input_cursor = itm.equipped
			});
			i++;
		}
		model->SetMenu( items_menu.get(), false );
		model->PushView( ViewType::gamemenu );
	} else {
		back();
		model->PopView();
	}
	signals->sig_new_frame();
}

void Controller::UpdateLoadGameMenu() {
	// fill load game menu with list of saved games
	auto load = [=](){
		// perform load game
		std::string savefile = model->GetMenu()->items[model->GetSelection()].name;
		model->GenerateMap();
		model->LoadGame(savefile);
		model->SetView(ViewType::game);
		signals->sig_new_game();
	};
	
	load_game->items.clear();
	
	// iterate savegames dir and fill Load Game menu with save games
	for(auto &dir : boost::filesystem::directory_iterator("savegames")) {
		load_game->items.push_back(MenuItem {
			MenuItem::Type::button, dir.path().string(), load
		});
	}
	auto back = [=]() {
		model->PopMenu();
	};
	load_game->items.push_back(MenuItem{MenuItem::Type::button, "<< Back", back});
	//
}

void Controller::ToggleSaveGameDialog() {
	static Menu save_game {
		"SAVE GAME",
		{
			{MenuItem::Type::inputfield, "Enter save filename: ", .max_input=15},
			{MenuItem::Type::button, "<< Back", [=](){model->SetView(ViewType::game);}},
		},
		[=](){
			std::string savefile = model->GetMenu()->items[model->GetSelection()].input;
			model->SaveGame("savegames/"+savefile);
			model->SetView(ViewType::game);
		}
	};
	
	model->SetMenu(&save_game);
	model->SetView(ViewType::gamemenu);
	signals->sig_new_frame();
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
			if(c == KEY_LEFT) { 
				// move cursor left
				item.input_cursor = std::max(0, item.input_cursor-1);
			} else if(c == KEY_RIGHT) { 
				// move cursor right
				item.input_cursor = std::min<int>(item.input.size(), item.input_cursor+1);
			} else if(c == KEY_BACKSPACE || c == '\b' || c == key_backspace) { 
				// remove character with backspace
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
			
			// enter while on inputfield will execute menu "submit" action
			if(c == '\n') {
				if(model->GetMenu()->onclick) model->GetMenu()->onclick();
			}
		} else {
			if(c == KEY_BACKSPACE || c == '\b' || c == key_backspace) {
				if(!model->PopMenu()) {
					if(!model->PopView() && model->GetMenu()->onback) {
						model->GetMenu()->onback();
					}
				}
			} else if(c == KEY_ENTER || c == '\n') {
				if(item.type == MenuItem::Type::button) {
					if(item.onclick) item.onclick();
				}
				if(item.type == MenuItem::Type::toggle) {
					item.input_cursor ^= 1;
					if(item.onclick) item.onclick();
				}
			}
		}
			
		signals->sig_new_frame();
	}
	
	// toggle items dialog
	
	if(c == 'i' && in(model->GetView(), {ViewType::game,ViewType::gamemenu})) {
		ToggleItemsDialog();
	}
	
	// save game dialog
	if(c == 19 && (model->GetView() == ViewType::game)) {
		ToggleSaveGameDialog();
	}
	
	// main menu dialog
	// TODO: fix ESC needs pressing twice or couple of times for its reaction
	if(c == 27 && (model->GetView() == ViewType::game)) {
		UpdateLoadGameMenu();
		model->SetMenu(main_menu2.get());
		model->PushView(ViewType::menu);
		signals->sig_new_frame();
	}
	
	// process arrow keys
	auto it = input_map.find(c);
	if(it != input_map.end()) {
		if(in(model->GetView(), {ViewType::menu,ViewType::gamemenu})) {
			if(!std::isalpha(c)) {
				model->IncrementSelection(it->second.y);
				signals->sig_new_frame();
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
