#pragma once
#include <boost/signals2.hpp>
struct Signals {
	boost::signals2::signal<void()> sig_newFrame;
	boost::signals2::signal<void()> sig_quit;
	boost::signals2::signal<void(int)> sig_input;
};
