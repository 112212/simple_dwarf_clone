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
// VEC_CMP_OPERATOR(==)
// VEC_CMP_OPERATOR(!=)


static bool isInRect(glm::ivec2 pt, glm::ivec2 topleft, glm::ivec2 size) {
	auto test = (pt > topleft) * (pt < topleft + size);
	return test.x && test.y;
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

static glm::ivec2 neg_mod(glm::ivec2 val, glm::ivec2 m) {
	return m*(val < 0) + val;
}

class VecIterate {
 public:
	using value = glm::ivec2;
	VecIterate(const glm::ivec2 &a, const glm::ivec2 &b) : m_start(a), m_end(b) {}

	const value& operator*() const {
		return m_start;
	}
	
	VecIterate& operator++() {
		if(m_start.x == m_end.y) {
			++m_start.y;
			m_start.x = m_end.x;
		} else {
			++m_start.x;
		}
		return *this;
	}
  
	bool operator!=(VecIterate& it) const {
		return m_start.x != it.m_start.x || m_start.y != it.m_start.y;
	}
  
	VecIterate begin() const {
		return VecIterate(m_start, {m_start.x, m_end.x-1});
	}

	VecIterate end() const {
		return VecIterate({m_start.x, m_end.y}, m_start);
	}

 private:
	glm::ivec2 m_start;
	glm::ivec2 m_end;
};
