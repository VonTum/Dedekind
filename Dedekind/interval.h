#pragma once

#include "functionInputBitSet.h"

template<unsigned int Variables>
struct Interval {
	FunctionInputBitSet<Variables> bot;
	FunctionInputBitSet<Variables> top;


	uint64_t intevalSize() const {
		if(!bot.isSubSetOf(top)) {
			return 0;
		} else if(top == bot) {
			return 1;
		} else {

		}
	}

};

