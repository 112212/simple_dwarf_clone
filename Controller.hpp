#pragma once
#include "Model.hpp"
#include "View.hpp"


class Controller {
public:
	Controller(Model* model, View* view);
	void Move(glm::ivec2 relpos);
	void InitMenus();
	void ProcessInput(int c);
	boost::signals2::signal<void()> sig_newFrame;
	boost::signals2::signal<void()> sig_quit;
private:
	Model* model;
};
