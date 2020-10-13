cpp := 	\
		Controller.cpp 	\
		Model.cpp		\
		View.cpp		\
		Main.cpp		\
		
build := build

flags := -g -O0
link := -lcurses

obj := $(addprefix $(build)/, $(patsubst %.cpp,%.o,$(cpp)))

exe := game

.PHONY: make_dir

all: make_dir $(exe)

make_dir:
	@mkdir -p build

DEP = $(obj:%.o=%.d)
-include $(DEP)

$(build)/%.o: %.cpp
	$(CXX) -c $< -MMD -o $@ $(flags) $(includes)
	
$(exe): $(obj)
	g++ $^ -o $@ $(link)
