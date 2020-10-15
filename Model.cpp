
#include "Model.hpp"
#include "libs/json.hpp"
#include <fstream>
#include <algorithm>
#include <random>

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
	
	glm::ivec2 playerPos = {75*Chunk::xsize,75*Chunk::ysize};
	
	// put some trees
	for(int i=0; i < 50; i++) {
		for(int j=0; j < 10; j++) {
			auto &trpos = GetObjectAt(playerPos + glm::ivec2(2*i,4+2*j));
			trpos.type = Object::Type::tree;
		}
	}
	
}

void Model::NewGame() {
	static std::default_random_engine re;
	std::uniform_int_distribution<int> unif(0,items.size());
	
	
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
		{0},{1},{2},{3}
	};
	
	for(int i = 0; i < 10; i++)
	{
		// put enemy
		auto pos = player->position + glm::ivec2(-2 + i*3, -3);
		auto &enpos = GetObjectAt(pos);
		enpos.type = Object::Type::enemy;
		auto en = std::make_shared<Actor>();
		en->position = pos;
		enemies.insert(en);
		enpos.obj = en;
		en->hp = 50;
		en->damage = 10;
			
		en->items = {
			{unif(re),false}
		};
	}	
	SetAttackedPos({-1,-1});
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


const ItemDef&	Model::GetItemDef(int idx) {
	return items[idx];
}

static nlohmann::json ActorToJson(Actor* actor) {
	using namespace nlohmann;
	json jactor;
	jactor["position"] = json::array({actor->position.x, actor->position.y});
	jactor["hp"] = actor->hp;
	jactor["armor"] = actor->armor;
	jactor["damage"] = actor->damage;
	
	jactor["items"] = json::array();
	
	for(auto& i : actor->items) {
		json jitem = json::array({i.idx, i.equipped});
		jactor["items"].push_back(jitem);
	}
	return jactor;
}

static std::shared_ptr<Actor> JsonToActor(nlohmann::json j) {
	using namespace nlohmann;
	json jactor;
	std::shared_ptr<Actor> actor = std::shared_ptr<Actor>(new Actor());
	actor->position = {j["position"][0],j["position"][1]};
	actor->hp = j["hp"];
	actor->armor = j["armor"];
	actor->damage = j["damage"];
	
	for(auto& i : j["items"]) {
		actor->items.push_back(Item{i[0], i[1]});
	}
	return actor;
}

void Model::SaveGame(std::string jsonFilename) {
	using namespace nlohmann;
	json j;
	
	j["player"] = ActorToJson(player.get());
	json jenemies = json::array();
	for(auto &e : enemies) {
		jenemies.push_back(ActorToJson(e.get()));
	}
	j["enemies"] = jenemies;
	std::ofstream f(jsonFilename);
	f.width(4);
	f << j;
}

void Model::LoadGame(std::string jsonFilename) {
	using namespace nlohmann;
	std::ifstream f(jsonFilename);
	json j;
	f >> j;
	for(auto &e : j["enemies"]) {
		auto en = JsonToActor(e);
		auto &plpos = GetObjectAt(en->position);
		plpos.type = Object::Type::enemy;
		plpos.obj = en;
		enemies.emplace(std::move(en));
	}
	
	player = JsonToActor(j["player"]);
	auto &plpos = GetObjectAt(player->position);
	plpos.type = Object::Type::friendly;
	plpos.obj = player;
}

void Model::LoadConfig(std::string jsonFilename) {
	using namespace nlohmann;
	std::fstream f(jsonFilename);
	json j;
	f >> j;
		
	// load palette definitions
	std::string palette = j["palette"];
	std::copy_n(palette.begin(), Object::Type::num_types, charPalette.begin());
	
	auto get = [](auto& j, std::string name, const auto &def) {
		auto it = j.find(name);
		return it != j.end() ? (decltype(def))*it : def;
	};
	
	// load item definitions
	{
		for(auto &i : j["items"]) {
			items.push_back(ItemDef{
				get(i, "char", std::string(" "))[0],
				get(i, "consumable", false),
				get(i, "damage", 0),
				get(i, "armor", 0),
				get(i, "hp", 0),
			});
		}
	}
}

const std::array<char, Object::Type::num_types>& Model::GetCharPalette() {
	return charPalette;
}


std::set<std::shared_ptr<Actor>>& Model::GetEnemies() {
	return enemies;
}

// ============= Menu ==============

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

void Model::PushMenu(Menu* menu) {
	menu_stack.push(current_menu);
	current_menu = menu;
	current_menu->Clear();
	m_selection = 0;
}

bool Model::PopMenu() {
	if(menu_stack.empty()) return false;
	current_menu = menu_stack.top();
	menu_stack.pop();
	m_selection = 0;
	return true;
}

void Menu::Clear() {
	for(auto &i : items) {
		i.input_cursor = 0;
		i.input.clear();
	}
}

// View

ViewType  Model::GetView() {
	return m_view;
}

void Model::SetView(ViewType view) {
	m_view = view;
	m_selection = 0;
	view_stack = std::stack<ViewType>();
}

void Model::PushView(ViewType type) {
	view_stack.push(m_view);
	m_selection = 0;
	m_view = type;
}

bool Model::PopView() {
	if(view_stack.empty()) return false;
	m_view = view_stack.top();
	view_stack.pop();
	return true;
}

// Selection

void Model::IncrementSelection(int dir) {
	m_selection = glm::clamp(m_selection + dir, 0, (int)current_menu->items.size()-1);
}

int Model::GetSelection() {
	return m_selection;
}



