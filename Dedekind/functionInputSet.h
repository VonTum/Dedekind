#pragma once

#include "functionInput.h"
#include "set.h"
#include "aligned_set.h"

typedef aligned_set<FunctionInput, 32> FunctionInputSet;

inline FunctionInput span(const FunctionInputSet& inputSet) {
	FunctionInput result{0};

	for(FunctionInput f : inputSet) {
		result |= f;
	}

	return result;
}

inline void swizzleVector(const std::vector<int>& permutation, const FunctionInputSet& input, FunctionInputSet& output) {
	for(size_t i = 0; i < input.size(); i++) {
		output[i] = input[i].swizzle(permutation);
	}
}

inline FunctionInputSet removeSuperInputs(const FunctionInputSet& layer, const FunctionInputSet& inputSet) {
	FunctionInputSet result;
	for(FunctionInput f : layer) {
		for(FunctionInput i : inputSet) {
			if(isSubSet(i, f)) {
				goto skip;
			}
		}
		result.push_back(f);
		skip:;
	}
	return result;
}
