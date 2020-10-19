#pragma once
#include <boost/signals2.hpp>
#include <glm/glm.hpp>
struct Signals {
	boost::signals2::signal<void()> sig_new_frame;
	boost::signals2::signal<void(glm::ivec2 size)> sig_canvas_size_changed;
	boost::signals2::signal<void()> sig_quit;
	boost::signals2::signal<void(int)> sig_input;
};
