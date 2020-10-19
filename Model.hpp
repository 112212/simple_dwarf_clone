#pragma once

#include <vector>
#include <map>
#include <array>
#include <memory>
#include <stack>
#include <set>

#include "Utils.hpp"
#include <glm/glm.hpp>
#include <glm/vector_relational.hpp>

#include <functional>

struct Object {
	enum Type {
		none,
		item,
		actor
	};
	Object(Type t=actor, glm::ivec2 pos={0,0}) : type(t), position(pos) {}
	Type type;
	glm::ivec2 position;
};

struct ItemDef {
	int charRepr;
	std::string name;
	bool consumable;
	int damage;
	int armor;
	int hp;
};

struct Item {
	int idx;
	bool equipped;
};

struct Actor : Object {
	Actor(glm::ivec2 pos) : Object(Object::Type::actor, pos) {}
	int hp;
	int armor;
	int damage;
	std::vector<Item> items;
};

struct ItemObject : Object {
	ItemObject() : Object(Object::Type::item) {}
	ItemObject(Item i, glm::ivec2 pos) : 
		Object(Object::Type::item, pos) {item = i;}
	Item item;
};


struct Tile {
	enum Type {
		empty,
		friendly,
		enemy,
		obstacle,
		tree,
		mountain,
		item,
		water,
		num_types
	};
	Type type;
	int elevation;
	Object* obj;
};

enum class ViewType {
	menu,
	game,
	gamemenu,
};

struct MenuItem {
	enum Type {
		button,
		inputfield,
		toggle
	};
	Type type;
	std::string name;
	std::function<void()> onclick;
	int max_input;
	int input_cursor;
	std::string input;
	
	int getWidth() const { return name.size() + max_input; }
	bool operator<(const MenuItem& b) const { return getWidth() < b.getWidth(); }
};

struct Menu {
	std::string title;
	std::vector<MenuItem> items;
	void Clear();
	std::function<void()> onclick;
	std::function<void()> onback; // if no menu in stack, we can to try use this callback
};

class Model {
public:
	Model();
	struct Chunk {
		static constexpr int xsize = 64;
		static constexpr int ysize = 64;
		std::array<Tile, xsize*ysize> tiles;

		Tile& TileAt(const glm::ivec2& pos) {
			return tiles[pos.y*xsize + pos.x];
		}
	};
	
	// map
	Chunk& 						GetChunk(const glm::ivec2& pos);
	Tile& 						GetTileAt(const glm::ivec2& pos);
	void						ForEachObject(std::function<void(Object*)> func);
	void 						RemoveObject(Object* pos);
	void 						InsertObject(Object* pos);
	
	Actor* 					GetPlayer();
	glm::ivec2 				GetPlayerPosition();
	
	// game making
	void 	ClearMap();
	void 	GenerateChunk(glm::ivec2 window);
	void	NewGame();
	void 	SaveGame(std::string jsonFilename);
	void 	LoadGame(std::string jsonFilename);
	void	LoadConfig(std::string jsonFilename);
	void	SetSeed(std::string seed);
	
	// which interface should render
	void 		SetView(ViewType view);
	ViewType	GetView() const;
	void		PushView(ViewType view);
	bool		PopView();
	
	// menu
	void		SetMenu(Menu* menu, bool clear=true);
	void		PushMenu(Menu* menu);
	bool		PopMenu();
	Menu*		GetMenu() const;
	int 		GetSelection() const;
	MenuItem& 	GetSelectedItem() const;
	void		IncrementSelection(int dir);
	
	// for combat system
	void		SetAttackedPos(glm::ivec2 pos);
	glm::ivec2  GetAttackedPos();
	
	
	const std::array<char, Tile::Type::num_types>& 	GetCharMap();
	const std::array<char, 4>& 						GetElevationMap();
	const ItemDef&		GetItemDef(int idx);

	void 				SetCanvasSize(const glm::ivec2& win_size);
	const glm::ivec2& 	GetCanvasSize() const;
	
	void 				SetCameraPos(const glm::ivec2& campos);
	const glm::ivec2& 	GetCameraPos() const;
	
	const static glm::ivec2 chunk_size;
	
	
private:
	uint32_t 				m_seed;
	std::unique_ptr<Actor> 	m_player;
	std::vector<ItemDef> 	m_item_defs;
	std::set<std::unique_ptr<Object>> 					m_objects;
	std::map<glm::ivec2, Chunk, vec2_cmp<glm::ivec2>> 	m_chunks;
	std::set<glm::ivec2, vec2_cmp<glm::ivec2>> 			m_generated_chunks;
	std::set<glm::ivec2, vec2_cmp<glm::ivec2>> 			m_objects_generated_chunks;
	std::array<char, Tile::Type::num_types> 			m_char_map;
	std::array<char, 4> 								m_elevation_map;
	
	glm::ivec2 m_camera_position;
	
	// non saveable state
	Menu* 					m_current_menu;
	std::stack<ViewType> 	m_view_stack;
	std::stack<Menu*> 		m_menu_stack;
	ViewType 				m_view;
	int 					m_selection;
	glm::ivec2				m_attacked_pos;
	glm::ivec2				m_canvas_size;
};
