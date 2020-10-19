#include "Controller.hpp"

#ifdef LINUX
#include <termios.h>
#endif

#ifdef NCURSES
#include <ncurses.h>
#else
#include "libs/PDCurses/curses.h"
#endif
#include <filesystem>
#include <cctype>
#include <chrono>
#include <thread>
#include <random>

#include <fstream>

static std::unique_ptr<Menu> main_menu;
static std::unique_ptr<Menu> main_menu2;
static std::unique_ptr<Menu> save_game;
static std::unique_ptr<Menu> load_game;
static std::unique_ptr<Menu> items_menu;
static std::unique_ptr<Menu> new_game;

auto noaction = [](){};

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
			{MenuItem::Type::inputfield, "Enter seed: ", noaction, 15},
			{MenuItem::Type::button, "<< Back", back},
		},
		[=]() {
			std::string seed = model->GetMenu()->items[0].input;
			model->ClearMap();
			if(!seed.empty()) {
				model->SetSeed(seed);
			}
			model->NewGame();
			model->SetView(ViewType::game);
			model->SetCameraPos(model->GetPlayerPosition());
			UpdateCamera();
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
			{MenuItem::Type::inputfield, "Enter save filename: ", noaction, 15},
			{MenuItem::Type::button, "<< Back", back},
		},
		[=](){
			std::string savefile = model->GetSelectedItem().input;
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
	signals->sig_canvas_size_changed.connect([=](glm::ivec2 new_size) {
		model->SetCanvasSize(new_size);
		UpdateCamera();
	});
	InitMainMenu();
	model->SetMenu(main_menu.get());
	model->SetView(ViewType::menu);
}

void Controller::DoDamage(Actor* a, Actor* b) {
	model->SetAttackedPos(a->position);
	a->hp 	 = std::max<int>(0, a->hp - b->damage * std::min<float>(1.0f, 5.0f/a->armor));
	a->armor = std::max<int>(0, a->armor - b->damage);
	
	if(a->armor == 0) {
		// erase equipped armor if armor gets to 0
		a->items.erase(std::remove_if(a->items.begin(), a->items.end(), [&](const Item& i) {
			return i.equipped && model->GetItemDef(i.idx).armor > a->armor;
		}), a->items.end());
	}
	
	signals->sig_new_frame();
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

bool Controller::Move(glm::ivec2 frompos, glm::ivec2 relpos) {
	glm::ivec2 old_pos = frompos;
	auto &old_place = model->GetTileAt(old_pos);
	// do we have anything movable to move?
	if(!in(old_place.type, {Tile::Type::enemy,Tile::Type::friendly})) return false;
	
	glm::ivec2 new_pos = old_pos + relpos;
	auto &place_to_go = model->GetTileAt(new_pos);
	
	if(in(place_to_go.type, {Tile::Type::empty, Tile::Type::item})) {
		
		// what to move
		auto player = static_cast<Actor*>(old_place.obj);
		
		// pickup any items if there
		if(place_to_go.type == Tile::Type::item) {
			player->items.push_back( static_cast<ItemObject*>(place_to_go.obj)->item );
			model->RemoveObject(place_to_go.obj);
		}
		
		// move to pos
		place_to_go.type = old_place.type;
		place_to_go.obj = old_place.obj;
		old_place.type = Tile::Type::empty;
		old_place.obj = 0;
		player->position = new_pos;
		
	} else {
		
		// attack enemy
		if(in(place_to_go.type, {Tile::Type::enemy, Tile::Type::friendly}) && place_to_go.type != old_place.type) {
			auto player = static_cast<Actor*>(old_place.obj);
			auto target = static_cast<Actor*>(place_to_go.obj);
			
			DoDamage(player, target);
			DoDamage(target, player);

			if(target->hp <= 0) {
				place_to_go.type = Tile::Type::empty;
				
				// drop some item if has any
				if(!target->items.empty()) {
					place_to_go.type = Tile::Type::item;
					auto item = new ItemObject(target->items.front(), new_pos);
					model->RemoveObject(place_to_go.obj);
					place_to_go.obj = item;
					model->InsertObject(item);
				} else {
					// remove from list
					model->RemoveObject(place_to_go.obj);
				}
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
		} else {
			return false;
		}
	}
	
	return true;
}

void Controller::UpdateCamera() {
	auto player = model->GetPlayer();
	if(player) {
		// move camera position if needed
		glm::ivec2 campos = model->GetCameraPos();
		glm::ivec2 canvas = model->GetCanvasSize();
		glm::ivec2 playerpos = model->GetPlayerPosition();
		glm::ivec2 dist   = glm::abs(playerpos-campos) >= canvas/2 * 3/4;
		model->SetCameraPos(campos + dist * (glm::sign(playerpos-campos)));
		model->SetAttackedPos(campos - canvas);
		
		// generate m_chunks if needed
		for(auto v : VecIterate((campos-canvas)/Model::chunk_size-1, (campos+canvas)/Model::chunk_size + 1)) {
			model->GenerateChunk(v);
		}
	}
}

void Controller::ToggleItemsDialog() {
	auto player = model->GetPlayer();
	auto back = [=](){
		// remove consumed items
		auto erased = std::remove_if(player->items.begin(), player->items.end(), [](const Item& itm) { return itm.idx == -1; });
		player->items.erase( erased, player->items.end() );
	};
	if(model->GetView() == ViewType::game) {
		// make new items menu
		items_menu = std::unique_ptr<Menu>(new Menu({ "ITEMS", {}, noaction, back }));
		// populate items menu with player items
		int i=0;
		for(auto& itm : player->items) {
			auto itm_def = model->GetItemDef(itm.idx);
			std::string name = std::string(itm_def.consumable ? "c" : "e") + " " + itm_def.name;
			items_menu->items.push_back(MenuItem {
				MenuItem::Type::toggle, name, [=](){
					auto& item = player->items[i];
					auto& item_def = model->GetItemDef(item.idx);
					
					// prevent equipping 2 items of same type
					if(!item_def.consumable && !item.equipped) {
						if(std::find_if(player->items.begin(), player->items.end(), [=](const Item& itm1) {
							if(itm1.idx != -1 && itm1.equipped) {
								auto itm1_def = model->GetItemDef(itm1.idx);
								return (bool)(item_def.damage * itm1_def.damage + item_def.hp * itm1_def.hp + item_def.armor * itm1_def.armor);
							}
							return false;
						}) != player->items.end()) {
							model->GetSelectedItem().input_cursor = 0; // block menu toggle effect
							return;
						}
					}
					
					// if player has less armor than equipped armor, then unequipping will destroy such item
					bool consumable = item_def.consumable || (item.equipped && item_def.armor > player->armor);
					item.equipped 	^= true; // toggle equip
					
					
					// equip item or unequip it
					player->hp 		= glm::max(0, player->hp 	 + (item.equipped ? 1 : -1) * item_def.hp);
					player->armor 	= glm::max(0, player->armor  + (item.equipped ? 1 : -1) * item_def.armor);
					player->damage 	= glm::max(0, player->damage + (item.equipped ? 1 : -1) * item_def.damage);
					
					// remove item if consumable
					if(consumable) {
						item.idx = -1; // mark for removal (must keep indices until inventory is closed)
						items_menu->items.erase(items_menu->items.begin() + model->GetSelection());
						model->IncrementSelection(-1); // move cursor back
					}
				},
				0,
				itm.equipped
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
		// load game
		std::string savefile = model->GetSelectedItem().name;
		model->ClearMap();
		model->LoadGame(savefile);
		model->ClearMap();
		model->LoadGame(savefile);
		UpdateCamera();
		model->SetView(ViewType::game);
	};
	
	load_game->items.clear();
	
	// iterate savegames dir and fill Load Game menu with save games
	for(auto &dir : std::filesystem::directory_iterator("savegames")) {
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
			{MenuItem::Type::inputfield, "Enter save filename: ", noaction, 15},
			{MenuItem::Type::button, "<< Back", [=](){model->SetView(ViewType::game);}},
		},
		[=](){
			std::string savefile = model->GetSelectedItem().input;
			model->SaveGame("savegames/"+savefile+".json");
			model->SetView(ViewType::game);
		}
	};
	
	model->SetMenu(&save_game);
	model->SetView(ViewType::gamemenu);
	signals->sig_new_frame();
}

void Controller::ProcessInput(int c) {
	
	static std::map<int, glm::ivec2> input_map {
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
	
	// handle menu controls
	if(in(model->GetView(), {ViewType::menu,ViewType::gamemenu})) {
		auto& item = model->GetSelectedItem();
		
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
			} else {
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
			if(c == KEY_BACKSPACE || c == '\b' || c == key_backspace) { // go back with backspace
				if(!model->PopMenu()) {
					if(!model->PopView() && model->GetMenu()->onback) {
						model->GetMenu()->onback();
					}
				}
			} else if(c == KEY_ENTER || c == '\n') { // submit/toggle with enter
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
	
	// toggle items dialog (i key)
	if(c == 'i' && in(model->GetView(), {ViewType::game,ViewType::gamemenu})) {
		ToggleItemsDialog();
	}
	
	// save game dialog (ctrl-s)
	if(c == 19 && (model->GetView() == ViewType::game)) {
		ToggleSaveGameDialog();
	}
	
	// main menu dialog (ESC)
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
			// move player
			if(Move(model->GetPlayerPosition(), it->second)) { // if player can't move that direction, don't move enemies either
				
				// we moved, update our camera if needed
				UpdateCamera();
				
				// move enemies randomly, after player moves
				static std::default_random_engine re;
				std::uniform_int_distribution<int> unif(0,4);
				std::string keys = "wasd";
				model->ForEachObject([&](Object* o) {
					if(o->type == Object::Type::actor) {
						int move_choice = unif(re);
						if(move_choice == 4) return; // stand in place
						Move(o->position, input_map[keys[move_choice]]);
					}
				});
			}
			signals->sig_new_frame();
		}
	}
}
