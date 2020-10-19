cpp := 	\
		Controller.cpp 	\
		Model.cpp		\
		View.cpp		\
		ViewColors.cpp	\
		libs/OpenSimplexNoise/OpenSimplexNoise/OpenSimplexNoise.cpp	\
		Main.cpp		\
		
build := build
use_ncurses := true

flags := -g -O2 -std=c++17 -Ilibs

ifeq ($(use_ncurses),true)
	flags += -D NCURSES
	link := -lncurses
else
	link := -L. -lpdcurses -lSDL2
endif

obj := $(addprefix $(build)/, $(patsubst %.cpp,%.o,$(cpp)))

exe := game

.PHONY: make_dir

all: make_dir $(exe)

make_dir:
	@mkdir -p $(build)
	@mkdir -p $(build)/libs/OpenSimplexNoise/OpenSimplexNoise

DEP = $(obj:%.o=%.d)
-include $(DEP)

$(build)/%.o: %.cpp
	$(CXX) -c $< -MMD -o $@ $(flags) $(includes)
	
$(exe): $(obj)
	$(CXX) $^ -o $@ $(link)

clean:
	rm -rf $(build)
	rm -f $(exe)
