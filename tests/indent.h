#pragma once


#include <iostream>

class Scope {
public:
	static int indent;
	Scope() noexcept {
		Scope::indent++;
	}
	~Scope() noexcept {
		Scope::indent--;
	}
};
