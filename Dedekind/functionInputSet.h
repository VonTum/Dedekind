#pragma once

#include "functionInput.h"
#include "set.h"


FunctionInput span(const set<FunctionInput>& inputSet) {
	FunctionInput result{0};

	for(FunctionInput f : inputSet) {
		result |= f;
	}

	return result;
}

void swizzleVector(const std::vector<int>& permutation, const set<FunctionInput>& input, set<FunctionInput>& output) {
	for(size_t i = 0; i < input.size(); i++) {
		output[i] = input[i].swizzle(permutation);
	}
}

set<FunctionInput> removeSuperInputs(const set<FunctionInput>& layer, const set<FunctionInput>& inputSet) {
	set<FunctionInput> result;
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
