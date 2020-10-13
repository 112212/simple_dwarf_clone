#include "Model.hpp"
#include "View.hpp"
#include "Controller.hpp"

int main() {
	Model model;
	View view(&model);
	model.GenerateMap();
	view.Init();
	
	Controller controller(&model, &view);
	
	while(1) {
		view.GetInput();
	}
	return 0;
}
