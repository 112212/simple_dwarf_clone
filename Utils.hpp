#pragma once
#include <glm/glm.hpp>

// map vector compare operator
template<typename T = glm::ivec2>
struct vec2_cmp {
	bool operator() (const T& p1, const T& p2) const {
		return p1.x < p2.x || (p1.x == p2.x && p1.y < p2.y);
	}
};

// for debug
#include <iostream>
static std::ostream& operator<< (std::ostream& out, const glm::ivec2& v) {
	return out << "(" << v.x << ", " << v.y << ")";
}

// matlab style comparison operators
#define VEC_OPERATOR(op) \
template<typename T> \
static T operator op (const T& v1, const T& v2) { \
	return T(v1.x op v2.x, v1.y op v2.y); \
} \
template<typename T, typename B> \
static T operator op (const T& v1, const B& s) { \
	return T(v1.x op s, v1.y op s); \
}

VEC_OPERATOR(>)
VEC_OPERATOR(>=)
VEC_OPERATOR(==)
VEC_OPERATOR(!=)


static bool str_size_cmp(const std::string& a, const std::string &b) {
	return a.size() > b.size(); 
};

template<typename It, typename Cmp>
auto max_val(It begin, It end, Cmp cmp) {
	if(begin == end) return 0;
	decltype(cmp(*begin)) vmax = cmp(*begin);
	for(It it = begin+1; it != end; ++it) {
		vmax = std::max(cmp(*it), vmax);
	}
	return vmax;
}