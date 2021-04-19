#pragma once

#include <bitset>

#include "functionInput.h"
#include "set.h"
#include "collectionOperations.h"
#include "smallVector.h"

//static_assert(MAX_PREPROCESSED == 7, "This size is the largest possible layer. ");
typedef SmallVector<FunctionInput, 35> FunctionInputSet;

inline FunctionInput span(const FunctionInputSet& inputSet) {
	FunctionInput result{0};

	for(FunctionInput f : inputSet) {
		result |= f;
	}

	return result;
}

inline bool isNormalized(const FunctionInputSet& fis) {
	FunctionInput sp = span(fis);

	return sp.getHighestEnabled() == sp.getNumberEnabled()-1;
}

template<typename PermutationVector>
inline void swizzleVector(const PermutationVector& permutation, const FunctionInputSet& input, FunctionInputSet& output) {
	for(size_t i = 0; i < input.size(); i++) {
		output[i] = input[i].swizzle(permutation);
	}
}

struct FullLayer : public FunctionInputSet {
	//using FunctionInputSet::FunctionInputSet;
	FullLayer() = default;
	explicit FullLayer(const FunctionInputSet& fi) : FunctionInputSet(fi) {}
	explicit FullLayer(FunctionInputSet&& fi) : FunctionInputSet(std::move(fi)) {}
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
inline FunctionInputSet removeForcedOn(const FullLayer& layer, const FunctionInputSet& inputSet) {
	FunctionInputSet result;
	for(FunctionInput f : layer) {
		if(!isForcedOnBy(f, inputSet)) {
			result.push_back(f);
		}
	}
	return result;
}

inline FunctionInputSet removeForcedOff(const FullLayer& layer, const FunctionInputSet& inputSet) {
	FunctionInputSet result;
	for(FunctionInput f : layer) {
		if(!isForcedOffBy(f, inputSet)) {
			result.push_back(f);
		}
	}
	return result;
}

// returns true if there is an element in the dominating input set which contains f
inline bool isDominatedBy(FunctionInput f, const FunctionInputSet& dominatingInputSet) {
	for(FunctionInput fi : dominatingInputSet) {
		if(isSubSet(f, fi)) {
			return true;
		}
	}
	return false;
}

inline FunctionInputSet getDominatedElements(const FunctionInputSet& layer, const FunctionInputSet& dominator) {
	FunctionInputSet result;
	for(FunctionInput fi : layer) {
		if(isDominatedBy(fi, dominator)) {
			result.push_back(fi);
		}
	}
	return result;
}
inline FunctionInputSet getNonDominatedElements(const FunctionInputSet& layer, const FunctionInputSet& dominator) {
	FunctionInputSet result;
	for(FunctionInput fi : layer) {
		if(!isDominatedBy(fi, dominator)) {
			result.push_back(fi);
		}
	}
	return result;
}

template<size_t Size>
inline FunctionInputSet invert(const std::bitset<Size>& toInvert, const FullLayer& everything) {
	FunctionInputSet result;
	result.reserve(everything.size() - toInvert.count());

	for(FunctionInput fi : everything) {
		if(!toInvert.test(fi)) {
			result.push_back(fi);
		}
	}

	return result;
}

template<size_t Size>
FunctionInputSet bitsetToFunctionInputSet(const std::bitset<Size>& bitSet) {
	FunctionInputSet result;
	result.reserve(bitSet.count());
	for(FunctionInput::underlyingType i = 0; i < Size; i++) {
		if(bitSet.test(i)) result.push_back(FunctionInput{i});
	}
	return result;
}

template<size_t Size>
std::bitset<Size> functionInputSetToBitSet(const FunctionInputSet& inputSet) {
	std::bitset<Size> result;
	for(FunctionInput fi : inputSet) {
		result.set(fi.inputBits);
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

// both FunctionInputSets must have been normalized (eg no unused variables)
inline FunctionInputSet findSubSetPermutation(const FunctionInputSet& set, const FunctionInputSet& subSet) {
	assert(isNormalized(set));
	assert(isNormalized(subSet));
	std::vector<int> permutation = generateIntegers(span(subSet).getNumberEnabled());

	FunctionInputSet permutedSubSet(permutation.size());
	bool found = existsPermutationForWhich(permutation, [&set, &subSet, &permutedSubSet](const std::vector<int>& permut) {
		permuteIntoExistingBuf(subSet, permut, permutedSubSet);
		return isSubSet(permutedSubSet.begin(), permutedSubSet.end(), set.begin(), set.end());
	});

	assert(found);

	return permutedSubSet;
}
