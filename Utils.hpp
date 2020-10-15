#pragma once
#include <glm/glm.hpp>
// #include <algorithm>
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
#define VEC_CMP_OPERATOR(op) \
static glm::ivec2 operator op (const glm::ivec2& v1, const glm::ivec2& v2) { \
	return glm::ivec2(v1.x op v2.x, v1.y op v2.y); \
} \
static glm::ivec2 operator op (const glm::ivec2& v1, const int& s) { \
	return glm::ivec2(v1.x op s, v1.y op s); \
}

VEC_CMP_OPERATOR(<)
VEC_CMP_OPERATOR(<=)
VEC_CMP_OPERATOR(>)
VEC_CMP_OPERATOR(>=)
VEC_CMP_OPERATOR(==)
VEC_CMP_OPERATOR(!=)

template<typename It, typename Cmp>
auto max_val(It begin, It end, Cmp cmp) {
	if(begin == end) return 0;
	decltype(cmp(*begin)) vmax = cmp(*begin);
	for(It it = begin+1; it != end; ++it) {
		vmax = std::max(cmp(*it), vmax);
	}
	return vmax;
}

template<typename T>
constexpr bool in(T val, const std::initializer_list<T>& list) {
	auto begin = list.begin(), end = list.end();
    while (begin != end) {
        if (*begin == val) return true;
        ++begin;
    }
    return false;
}
