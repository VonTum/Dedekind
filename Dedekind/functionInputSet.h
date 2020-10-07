#pragma once

#include <vector>

#include "functionInput.h"
#include "set.h"


FunctionInput span(const std::vector<FunctionInput>& inputSet) {
	FunctionInput result{0};

	for(FunctionInput f : inputSet) {
		result |= f;
	}

	return result;
}

void swizzleVector(const std::vector<int>& permutation, const std::vector<FunctionInput>& input, std::vector<FunctionInput>& output) {
	for(size_t i = 0; i < input.size(); i++) {
		output[i] = input[i].swizzle(permutation);
	}
}

std::vector<FunctionInput> removeSuperInputs(const std::vector<FunctionInput>& layer, const std::vector<FunctionInput>& inputSet) {
	std::vector<FunctionInput> result;
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
