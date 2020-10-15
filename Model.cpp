
#include "Model.hpp"
#include "libs/json.hpp"
#include <fstream>
#include <algorithm>

Model::Model() {
	m_view = ViewType::menu;
}

Model::Chunk& Model::GetChunk(glm::ivec2 pos) {
	return chunks[pos];
}


void Model::GenerateMap() {
	enemies.clear();
	
	// generate empty map
	for(int x=50; x < 100; x++)
	for(int y=50; y < 100; y++) {
		chunks[glm::ivec2(x,y)] = Chunk();
	}
	
	// put player on map
	player = std::make_shared<Actor>();
	glm::ivec2 playerPos = {75*Chunk::xsize,75*Chunk::ysize};
	auto &plpos = GetObjectAt(playerPos);
	plpos.type = Object::Type::friendly;
	plpos.obj = player;
	player->hp = 100;
	player->armor = 10;
	player->position = playerPos;
	player->damage = 40;
	
	player->items = {
		{'H',1,0,0,30},
		{'a',1,0,20,30},
		{'a',1,0,20,30},
		{'A',0,0,30,0},
	};
	
	for(int i=0; i < 50; i++) {
		for(int j=0; j < 10; j++) {
			// put some trees
			auto &trpos = GetObjectAt(playerPos + glm::ivec2(2*i,4+2*j));
			trpos.type = Object::Type::tree;
		}
	}
	for(int i = 0; i < 10; i++)
	{
		// put enemy
		auto pos = playerPos + glm::ivec2(-2 + i*3, -3);
		auto &enpos = GetObjectAt(pos);
		enpos.type = Object::Type::enemy;
		auto en = std::make_shared<Actor>();
		en->position = pos;
		enemies.insert(en);
		enpos.obj = en;
		en->hp = 50;
		en->damage = 10;
		en->items = {
			{'H',1,0,0,30},
		};
	}	
	SetAttackedPos({-1,-1});
}

void Model::MovePos(glm::ivec2 oldPos, glm::ivec2 newPos) {
	auto &place = GetObjectAt(oldPos);
	auto &placeToGo = GetObjectAt(newPos);
	place.obj.swap(placeToGo.obj);
	place.obj.reset();
	static_cast<Actor*>(placeToGo.obj.get())->position = newPos;
}

glm::ivec2 Model::GetAttackedPos() {
	return m_attacked_pos;
}

void Model::SetAttackedPos(glm::ivec2 pos) {
	m_attacked_pos = pos;
}

std::shared_ptr<Actor> Model::GetPlayer() {
	return player;
}

glm::ivec2 Model::GetPlayerPosition() {
	return player->position;
}


Tile& Model::GetObjectAt(glm::ivec2 pos) {
	return GetChunk( pos / 8 ).ObjectAt(pos % 8);
}


static nlohmann::json ActorToJson(Actor* actor) {
	using namespace nlohmann;
	json jactor;
	jactor["position"] = json::array({actor->position.x, actor->position.y });
	jactor["hp"] = actor->hp;
	jactor["armor"] = actor->armor;
	jactor["damage"] = actor->armor;
	
	jactor["items"] = json::array();
	
	for(auto& itm : actor->items) {
		json jitem;
		jitem["charRepr"] = itm.charRepr;
		// jitem["
		jactor["items"].push_back(jitem);
	}
	return jactor;
}

void Model::SaveGame(std::string jsonFilename) {
	using namespace nlohmann;
	json j;
	
	json jplayer;
	jplayer["position"] = json::array({player->position.x, player->position.y });
	jplayer["hp"] = player->hp;
	jplayer["armor"] = player->armor;
	jplayer["damage"] = player->armor;
	
	
	j["player"] = ActorToJson(player.get());
	
	
	
	std::fstream f(jsonFilename);
	f << j;
}

void Model::LoadGame(std::string jsonFilename) {
	using namespace nlohmann;
	std::fstream f(jsonFilename);
	json j;
	f >> j;
	
	json jpos = j["player"]["position"];
	player->position = glm::ivec2(jpos[0], jpos[1]);
	
	
}

void Model::LoadConfig(std::string jsonFilename) {
	using namespace nlohmann;
	std::fstream f(jsonFilename);
	json j;
	f >> j;
		
	// load palette definitions
	std::string palette = j["palette"];
	std::copy_n(palette.begin(), Object::Type::num_types, charPalette.begin());
	
	// load item definitions
	{
		for(auto &i : j["items"]) {
			
		}
		
		// json jpos = j["player"]["position"];
		// player->position = glm::ivec2(jpos[0], jpos[1]);
	}
}

const std::array<char, Object::Type::num_types>& Model::GetCharPalette() {
	return charPalette;
}


std::set<std::shared_ptr<Actor>>& Model::GetEnemies() {
	return enemies;
}

// Menu

void Model::SetMenu(Menu* menu, bool clear) {
	current_menu = menu;
	menu_stack = std::stack<Menu*>();
	if(clear) {
		current_menu->Clear();
	}
}

Menu* Model::GetMenu() {
	return current_menu;
}

void Menu::Clear() {
	for(auto &i : items) {
		i.input_cursor = 0;
		i.input.clear();
	}
}

void Model::PushMenu(Menu* menu) {
	menu_stack.push(current_menu);
	current_menu = menu;
	current_menu->Clear();
}

void Model::PopMenu() {
	if(menu_stack.empty()) return;
	current_menu = menu_stack.top();
	menu_stack.pop();
}

void Model::IncrementSelection(int dir) {
	m_selection = glm::clamp(m_selection + dir, 0, (int)current_menu->items.size()-1);
}

int Model::GetSelection() {
	return m_selection;
}

void Model::SetView(ViewType view) {
	m_view = view;
	m_selection = 0;
}

ViewType  Model::GetView() {
	return m_view;
}
