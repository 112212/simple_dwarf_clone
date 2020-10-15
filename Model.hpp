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
	virtual ~Object() {}
	enum Type {
		empty,
		friendly,
		enemy,
		obstacle,
		tree,
		mountain,
		item,
		num_types
	};
	
};

struct ItemDef {
	int charRepr;
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
	glm::ivec2 position;
	int hp;
	int armor;
	int damage;
	std::vector<Item> items;
};

struct ItemObject : Object {
	ItemObject(Item i) : item(i) {}
	Item item;
};


struct Tile {
	Object::Type type;
	int elevation;
	std::shared_ptr<Object> obj;
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
	
	int input_cursor;
	std::string input;
	int max_input;
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
		static constexpr int xsize = 8;
		static constexpr int ysize = 8;
		std::array<Tile, xsize*ysize> tiles;

		Tile& ObjectAt(const glm::ivec2& pos) {
			return tiles[pos.y*xsize + pos.x];
		}
	};
	
	// map
	Chunk& 	GetChunk(glm::ivec2 pos);
	Tile& 	GetObjectAt(glm::ivec2 pos);
	
	std::shared_ptr<Actor> 	GetPlayer();
	glm::ivec2 				GetPlayerPosition();
	
	// game making
	void 	GenerateMap();
	void	NewGame();
	void 	SaveGame(std::string jsonFilename);
	void 	LoadGame(std::string jsonFilename);
	void	LoadConfig(std::string jsonFilename);
	
	// which interface should render
	void 		SetView(ViewType view);
	ViewType	GetView();
	void		PushView(ViewType view);
	bool		PopView();
	
	// menu
	void		SetMenu(Menu* menu, bool clear=true);
	void		PushMenu(Menu* menu);
	bool		PopMenu();
	Menu*		GetMenu();
	int 		GetSelection();
	void		IncrementSelection(int dir);
	
	// for combat system
	void		SetAttackedPos(glm::ivec2 pos);
	glm::ivec2  GetAttackedPos();
	
	std::set<std::shared_ptr<Actor>>& 					GetEnemies();
	const std::array<char, Object::Type::num_types>& 	GetCharPalette();
	const ItemDef&	GetItemDef(int idx);

private:
	std::shared_ptr<Actor> player;
	std::set<std::shared_ptr<Actor>> enemies;
	std::vector<ItemDef> items;
	std::map<glm::ivec2, Chunk, vec2_cmp<glm::ivec2>> chunks;
	std::array<char, Object::Type::num_types> charPalette;
	
	// non saveable state
	Menu* 					current_menu;
	std::stack<ViewType> 	view_stack;
	std::stack<Menu*> 		menu_stack;
	ViewType 				m_view;
	int 					m_selection;
	glm::ivec2				m_attacked_pos;
};
