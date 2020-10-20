#pragma once

#include "iteratorEnd.h"

template<typename Iter>
struct IteratorFactory {
	Iter iter;

	Iter begin() const {
		return iter;
	}
	IteratorEnd end() const {
		return IteratorEnd{};
	}

	template<typename... Args>
	IteratorFactory(Args... args) : iter(args) {}
};
