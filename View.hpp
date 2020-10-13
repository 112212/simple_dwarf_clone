#pragma once
#include "Model.hpp"

struct _win_st;
typedef struct _win_st WINDOW;



class View {
public:
	View(Model* model);
	~View();
	void Init();
	void GetInput();
	void Render();
	void Quit();
	boost::signals2::signal<void(int)> sig_input;
	
private:

	void drawRect(glm::ivec2 center, glm::ivec2 size);
	void putString(glm::ivec2 center, std::string str, int cursor=-1);
	void renderMenu();
	void renderGame();
	void renderItemsMenu();
	
	Model* model;
	WINDOW* m_window;
	WINDOW* m_game_window;
	WINDOW* m_menu_window;
	glm::ivec2 m_window_size;
	glm::ivec2 m_camera_position;
};
