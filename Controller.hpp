#pragma once
#include "Model.hpp"
#include "View.hpp"
#include "Signals.hpp"

class Controller {
public:
	Controller(Model* _model, Signals* _signals);
	void Move(glm::ivec2 frompos, glm::ivec2 relpos);
	void ToggleItemsDialog();
	void InitMenus();
	void ProcessInput(int c);
private:
	Model* model;
	Signals* signals;
};
