#include "Model.hpp"
#include "View.hpp"
#include "Controller.hpp"

int main() {
	Model model;
	Signals signals;
	View view(&model, &signals);
	model.GenerateMap();
	view.Init();
	
	model.LoadConfig("config/config.json");
	Controller controller(&model, &signals);
	while(1) {
		view.GetInput();
	}
	return 0;
}
