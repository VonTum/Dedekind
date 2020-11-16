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

struct FullLayer : public FunctionInputSet {
	//using FunctionInputSet::FunctionInputSet;
	FullLayer() = default;
	explicit FullLayer(const FunctionInputSet& fi) : FunctionInputSet(fi) {}
};

// returns true if there is an item in onInputSet which is a superset of f
inline bool isForcedOnBy(FunctionInput f, const FunctionInputSet& onInputSet) {
	for(FunctionInput fi : onInputSet) {
		if(isSubSet(fi, f)) {
			return true;
		}
	}
	return false;
}

// returns true if there is an item in offInputSet which is a subset of f
inline bool isForcedOffBy(FunctionInput f, const FunctionInputSet& offInputSet) {
	for(FunctionInput fi : offInputSet) {
		if(isSubSet(f, fi)) {
			return true;
		}
	}
	return false;
}

// returns a new FunctionInputSet containing all the functionInputs that can still be chosen after 
inline FunctionInputSet removeForcedOn(const FunctionInputSet& layer, const FunctionInputSet& inputSet) {
	FunctionInputSet result;
	for(FunctionInput f : layer) {
		if(!isForcedOnBy(f, inputSet)) {
			result.push_back(f);
		}
	}
	return result;
}

inline FunctionInputSet removeForcedOff(const FunctionInputSet& layer, const FunctionInputSet& inputSet) {
	FunctionInputSet result;
	for(FunctionInput f : layer) {
		if(!isForcedOffBy(f, inputSet)) {
			result.push_back(f);
		}
	}
	return result;
}

inline FunctionInputSet invert(const FunctionInputSet& toInvert, const FullLayer& everything) {
	FunctionInputSet result(everything.size() - toInvert.size());

	size_t curIndex = 0;
	for(FunctionInput fi : everything) {
		if(!toInvert.contains(fi)) {
			result[curIndex++] = fi;
		}
	}

	return result;
}
