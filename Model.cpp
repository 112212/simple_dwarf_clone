
#include "Model.hpp"
#include "libs/json.hpp"
#include <fstream>
#include <algorithm>
#include <random>
#include <chrono>

#include <stdint.h>
#include "OpenSimplexNoise/OpenSimplexNoise/OpenSimplexNoise.h"

Model::Model() {
	m_view = ViewType::menu;
	m_seed = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
}

Model::Chunk& Model::GetChunk(const glm::ivec2& pos) {
	auto it = m_chunks.find(pos);
	if(it == m_chunks.end()) {
		m_chunks[pos] = Chunk();
		it = m_chunks.find(pos);
	}	
	return it->second;
}

Tile& Model::GetTileAt(const glm::ivec2& pos) {
	static glm::ivec2 chunk_size(Chunk::xsize,Chunk::ysize);
	glm::ivec2 chunk = pos / chunk_size;
	glm::ivec2 lpos = pos % chunk_size;
	
	// use negative modulus so we can properly access negative values
	chunk -= (lpos < 0);
	lpos = (lpos < 0) * chunk_size + lpos;
	
	return GetChunk(chunk).TileAt(lpos);
}

void Model::RemoveObject(Object* obj) {
	std::unique_ptr<Object> pobj(obj);
	auto it = m_objects.find(pobj);
	if(it != m_objects.end()) (*it)->type = Object::none;
	pobj.release(); // don't free
}

void Model::InsertObject(Object* pos) {
	m_objects.insert(std::unique_ptr<Object>(pos));
}

void Model::ForEachObject(std::function<void(Object*)> func) {
	for(auto it = m_objects.begin(); it != m_objects.end(); ) {
		if((*it)->type != Object::none) {
			func(it->get());
			it++;
		} else {
			it = m_objects.erase(it);
		}
	}
}


void Model::ClearMap() {
	m_chunks.clear();
	m_objects.clear();
	m_generated_chunks.clear();
}

void Model::NewGame() {
	// put player on map
	glm::ivec2 playerPos = {0*Chunk::xsize, 0*Chunk::ysize};
	m_player = std::make_unique<Actor>(playerPos);
	auto &plpos = GetTileAt(playerPos);
	plpos.type = Tile::Type::friendly;
	plpos.obj = m_player.get();
	m_player->hp = 100;
	m_player->armor = 10;
	m_player->damage = 40;
	
	// starting items
	m_player->items = {
		{0},{1}
	};
}

const glm::ivec2 Model::chunk_size = {Model::Chunk::xsize, Model::Chunk::ysize};

void Model::GenerateChunk(glm::ivec2 tl_chunk) {
	if(m_generated_chunks.find(tl_chunk) != m_generated_chunks.end()) return;
	m_generated_chunks.insert(tl_chunk);
	std::default_random_engine re;
	std::uniform_int_distribution<int> object_unif(0,1000);
	std::uniform_int_distribution<int> item_unif(0, m_item_defs.size()-1);
	
	OpenSimplexNoise::Noise simplex(m_seed);
	
	// if enemies are already loaded from save file, we must ignore that part
	bool gen_enemies = m_objects_generated_chunks.find(tl_chunk) == m_objects_generated_chunks.end();
	
	for(const auto &pos : VecIterate(tl_chunk*chunk_size, (tl_chunk+1)*chunk_size)) {
		re.seed(m_seed * std::max(1, std::abs(pos.x * pos.y)));
		
		// elevation
		auto &o = GetTileAt(pos);
		glm::vec2 v = glm::vec2(pos) / 33.0f;
		o.elevation = glm::clamp<int>(4.0 * simplex.eval(v.x, v.y), -2, 2);
		
		// trees
		std::array<float, 5> tree_chance {
			0.0,0.7,0.8,0.2,0.1
		};
		glm::vec2 v2 = glm::vec2(pos)/15.0f;
		if(o.type == Tile::empty) {
			o.type = simplex.eval(v2.x, v2.y) > (1.0-tree_chance[o.elevation+2]) ? Tile::tree : Tile::empty;
		}
		
		// elevation 2 is mountain
		if(o.elevation == 2 && o.type == Tile::empty) {
			o.type = Tile::mountain;
		}
		
		// elevation -2 is water
		if(o.elevation == -2) {
			o.type = Tile::water;
		}
		
		// enemies
		if( gen_enemies && o.type == Tile::empty && object_unif(re) < 8 ) {
			if(object_unif(re) < 700) { // 7/10 chance place enemy
				auto en = std::make_unique<Actor>(pos);
				o.type = Tile::enemy;
				o.obj = en.get();
				en->hp = 50;
				en->damage = 10;
				// 9/10 enemies drop item
				if(object_unif(re) < 900) {
					en->items = {
						{item_unif(re),false}
					};
				}
				m_objects.insert(std::move(en));
			} else { // 3/10 chance place item
				Item item;
				item.idx = item_unif(re);
				auto en = std::make_unique<ItemObject>(item, pos);
				o.type = Tile::item;
				o.obj = en.get();
				m_objects.insert(std::move(en));
			}
		}
	}
}



glm::ivec2 Model::GetAttackedPos() {
	return m_attacked_pos;
}

void Model::SetAttackedPos(glm::ivec2 pos) {
	m_attacked_pos = pos;
}

Actor* Model::GetPlayer() {
	return m_player.get();
}

glm::ivec2 Model::GetPlayerPosition() {
	return m_player->position;
}


// ====== SAVING AND LOADING ============

const ItemDef&	Model::GetItemDef(int idx) {
	return m_item_defs[idx];
}

static nlohmann::json v2j(glm::ivec2 v) {
	return {v.x, v.y};
}
static glm::ivec2 j2v(const nlohmann::json& v) {
	return {v[0], v[1]};
}
	
static nlohmann::json ObjectToJson(Object* object) {
	using namespace nlohmann;
	json jobject;
	jobject["type"] = object->type;
	jobject["position"] = v2j(object->position);
	if(object->type == Object::Type::item) {
		jobject["idx"] = static_cast<ItemObject*>(object)->item.idx;
	} else {
		Actor* actor = static_cast<Actor*>(object);
		
		jobject["hp"] = actor->hp;
		jobject["armor"] = actor->armor;
		jobject["damage"] = actor->damage;
		
		jobject["items"] = json::array();
		
		for(auto& i : actor->items) {
			json jitem = json::array({i.idx, i.equipped});
			jobject["items"].push_back(jitem);
		}
	}
	return jobject;
}

static Object* JsonToObject(nlohmann::json j) {
	using namespace nlohmann;
	json jactor;
	Object* object;
	if(j["type"] == Object::Type::item) {
		Item item;
		item.idx = j["idx"];
		ItemObject* itemobj = new ItemObject(item, j2v(j["position"]));
		object = itemobj;
	} else {
		Actor* actor = new Actor(j2v(j["position"]));
		object = actor;
		actor->hp 		= j["hp"];
		actor->armor 	= j["armor"];
		actor->damage 	= j["damage"];
		
		for(auto& i : j["items"]) {
			actor->items.push_back(Item{i[0], i[1]});
		}
	}
	return object;
}

void Model::SaveGame(std::string jsonFilename) {
	using namespace nlohmann;
	json j;
	
	j["player"] = ObjectToJson(m_player.get());
	json jobjects = json::array();
	ForEachObject([&](Object* o) {
		jobjects.push_back(ObjectToJson(o));
	});
	j["m_objects"] = jobjects;
	j["seed"] = m_seed;
	j["camera_position"] = v2j(m_camera_position);
	j["m_generated_chunks"] = json::array();
	m_objects_generated_chunks.insert(m_generated_chunks.begin(), m_generated_chunks.end());
	for(auto &ch : m_objects_generated_chunks) {
		j["m_generated_chunks"].push_back(v2j(ch));
	}
	std::ofstream f(jsonFilename);
	f.width(4);
	f << j;
}

void Model::LoadGame(std::string jsonFilename) {
	using namespace nlohmann;
	std::ifstream f(jsonFilename);
	json j;
	f >> j;
	m_seed = j["seed"];
	m_camera_position = j2v(j["camera_position"]);
	for(auto &e : j["m_objects"]) {
		auto en = std::unique_ptr<Object>(JsonToObject(e));
		auto &plpos = GetTileAt(en->position);
		plpos.type = en->type == Object::actor ? Tile::Type::enemy : Tile::item;
		plpos.obj = en.get();
		m_objects.insert(std::move(en));
	}
	auto gen_chunks = j["m_generated_chunks"];
	for(auto &ch : gen_chunks) {
		m_objects_generated_chunks.insert(j2v(ch));
	}
	m_player = std::unique_ptr<Actor>(static_cast<Actor*>(JsonToObject(j["player"])));
	auto &plpos = GetTileAt(m_player->position);
	plpos.type = Tile::Type::friendly;
	plpos.obj = m_player.get();
}

void Model::SetSeed(std::string seed) {
	std::seed_seq seq(seed.begin(), seed.end());
	std::vector<std::uint32_t> seeds(1);
	seq.generate(seeds.begin(), seeds.end());
	m_seed = seeds.front();
}

void Model::LoadConfig(std::string jsonFilename) {
	using namespace nlohmann;
	std::fstream f(jsonFilename);
	json j;
	f >> j;
		
	// load palette definitions
	std::string charmap = j["charmap"];
	std::string elev_map = j["elevationmap"];
	std::copy_n(charmap.begin(), Tile::Type::num_types, m_char_map.begin());
	std::copy_n(elev_map.begin(), 4, m_elevation_map.begin());
	
	auto get = [](auto& j, std::string name, const auto &def) {
		auto it = j.find(name);
		return it != j.end() ? (decltype(def))*it : def;
	};
	
	// load item definitions
	{
		for(auto &i : j["items"]) {
			m_item_defs.push_back(ItemDef{
				get(i, "char", std::string(" "))[0],
				get(i, "name", std::string(" ")),
				get(i, "consumable", false),
				get(i, "damage", 0),
				get(i, "armor", 0),
				get(i, "hp", 0),
			});
		}
	}
}

const std::array<char, Tile::Type::num_types>& Model::GetCharMap() {
	return m_char_map;
}

const std::array<char, 4>& Model::GetElevationMap() {
	return m_elevation_map;
}


// ============= Menu ==============

// Menu

void Model::SetMenu(Menu* menu, bool clear) {
	m_current_menu = menu;
	m_menu_stack = std::stack<Menu*>();
	if(clear) {
		m_current_menu->Clear();
	}
}

Menu* Model::GetMenu() const {
	return m_current_menu;
}

void Model::PushMenu(Menu* menu) {
	m_menu_stack.push(m_current_menu);
	m_current_menu = menu;
	m_current_menu->Clear();
	m_selection = 0;
}

bool Model::PopMenu() {
	if(m_menu_stack.empty()) return false;
	m_current_menu = m_menu_stack.top();
	m_menu_stack.pop();
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

void Model::SetCanvasSize(const glm::ivec2& canvas) {
	m_canvas_size = canvas;
}

const glm::ivec2& Model::GetCanvasSize() const {
	return m_canvas_size;
}

void Model::SetCameraPos(const glm::ivec2& campos) {
	m_camera_position = campos;
}

const glm::ivec2& Model::GetCameraPos() const {
	return m_camera_position;
}
	
ViewType  Model::GetView() const {
	return m_view;
}

void Model::SetView(ViewType view) {
	m_view = view;
	m_selection = 0;
	m_view_stack = std::stack<ViewType>();
}

void Model::PushView(ViewType type) {
	m_view_stack.push(m_view);
	m_selection = 0;
	m_view = type;
}

bool Model::PopView() {
	if(m_view_stack.empty()) return false;
	m_view = m_view_stack.top();
	m_view_stack.pop();
	return true;
}

// Selection

void Model::IncrementSelection(int dir) {
	m_selection = glm::clamp(m_selection + dir, 0, (int)m_current_menu->items.size()-1);
}

int Model::GetSelection() const {
	return m_selection;
}

MenuItem& Model::GetSelectedItem() const {
	return m_current_menu->items[m_selection];
}



