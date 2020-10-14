#pragma once

#include <vector>
#include <map>
#include <array>
#include <memory>
#include <stack>

#include "Utils.hpp"
#include <glm/glm.hpp>

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
		num_types
	};
	Type type;
	int elevation;
};

struct Item {
	int charRepr;
};

struct Actor : Object {
	glm::ivec2 position;
	int hp;
	int armor;
	int damage;
	std::vector<std::unique_ptr<Item>> items;
};

struct ItemObject : Object {
	std::unique_ptr<Item> item;
};

struct Weapon : Item {
	int damage;
};
struct Armor : Item {
	int armor;
};
struct Consumable : Item {
	int hp;
};

struct Tile {
	Object::Type type;
	std::shared_ptr<Object> obj;
};

enum class ViewType {
	menu,
	game,
	items
};

struct MenuItem {
	enum Type {
		button,
		inputfield
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
	
	Chunk& GetChunk(glm::ivec2 pos);
	Tile& GetObjectAt(glm::ivec2 pos);
	
	std::shared_ptr<Actor> 	GetPlayer();
	glm::ivec2 				GetPlayerPosition();
	
	void GenerateMap();
	
	void 		MovePos(glm::ivec2 oldPos, glm::ivec2 newPos);
	
	void 		SaveGame(std::string jsonFilename);
	void 		LoadGame(std::string jsonFilename);
	void		LoadConfig(std::string jsonFilename);
	
	void 		SetView(ViewType view);
	ViewType  	GetView();
	
	void		SetMenu(Menu* menu);
	void		PushMenu(Menu* menu);
	void		PopMenu();
	Menu*		GetMenu();
	int 		GetSelection();
	void		IncrementSelection(int dir);
	const std::array<char, Object::Type::num_types>& GetCharPalette();

private:
	std::shared_ptr<Actor> player;
	std::map<glm::ivec2, Chunk, vec2_cmp<glm::ivec2>> chunks;
	std::array<char, Object::Type::num_types> charPalette;
	// non saveable state
	Menu* 				current_menu;
	std::stack<Menu*> 	menu_stack;
	ViewType 			m_view;
	int 				m_selection;
};
