#pragma once
#include "Model.hpp"
#include "Signals.hpp"

class Controller {
public:
	Controller(Model* _model, Signals* _signals);
	bool Move(glm::ivec2 frompos, glm::ivec2 relpos);
	void DoDamage(Actor* a, Actor* b);
	void ToggleItemsDialog();
	void ToggleSaveGameDialog();
	void UpdateLoadGameMenu();
	void UpdateCamera();
	void InitMainMenu();
	void ProcessInput(int c);
private:
	Model* 	 model;
	Signals* signals;
};
