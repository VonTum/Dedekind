#pragma once

#include "iteratorEnd.h"

template<typename Iter>
struct IteratorFactoryWithEnd {
	Iter iter;

	Iter begin() const {
		return iter;
	}
	IteratorEnd end() const {
		return IteratorEnd{};
	}

	template<typename... Args>
	IteratorFactoryWithEnd(Args... args) : iter(args...) {}
};

template<typename Iter, typename IterEnd = Iter>
struct IteratorFactory {
	Iter iter;
	IterEnd iterEnd;

	Iter begin() const {
		return iter;
	}
	IterEnd end() const {
		return iterEnd;
	}

	IteratorFactory(Iter iter, IterEnd iterEnd) : iter(std::move(iter)), iterEnd(std::move(iterEnd)) {}
};
