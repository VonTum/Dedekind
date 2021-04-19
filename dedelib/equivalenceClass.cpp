#include "equivalenceClass.h"

#include "crossPlatformIntrinsics.h"

#include <immintrin.h>

#include <iostream>

PreprocessedFunctionInputSet PreprocessedFunctionInputSet::emptyPreprocessedFunctionInputSet = PreprocessedFunctionInputSet{FunctionInputSet{}};
EquivalenceClass EquivalenceClass::emptyEquivalenceClass = EquivalenceClass(PreprocessedFunctionInputSet::emptyPreprocessedFunctionInputSet);

PreprocessedFunctionInputSet PreprocessedFunctionInputSet::bottomPreprocessedFunctionInputSet = PreprocessedFunctionInputSet{FunctionInputSet{FunctionInput{0}}};
EquivalenceClass EquivalenceClass::bottomEquivalenceClass = EquivalenceClass(PreprocessedFunctionInputSet::bottomPreprocessedFunctionInputSet);

PreprocessedFunctionInputSet PreprocessedFunctionInputSet::extendedBy(FunctionInput fi) const {
	InputBitSet result = this->functionInputSet;
	result.add(fi);
	return PreprocessedFunctionInputSet{result.canonize()};
}

PreprocessedFunctionInputSet EquivalenceClass::extendedBy(FunctionInput fi) const {
	InputBitSet result = this->functionInputSet;
	result.add(fi);
	return PreprocessedFunctionInputSet{result.canonize()};
}

uint64_t PreprocessedFunctionInputSet::hash() const {
	return functionInputSet.hash();
}

FunctionInputSet EquivalenceClass::asFunctionInputSet() const {
	FunctionInputSet result;
	unsigned int sp = this->functionInputSet.span();
	assert(sp == 0 || sp == (1U << popcnt32(sp)) - 1);
	for(FunctionInput::underlyingType i = 0; i <= sp; i++) {
		if(functionInputSet.contains(i)) result.push_back(FunctionInput{i});
	}
	return result;
}

PreprocessedFunctionInputSet preprocess(FunctionInputSet inputSet) {
	return PreprocessedFunctionInputSet{InputBitSet(inputSet).canonize()};
}

template<typename Func>
bool existsSubPermutationRecursive(const int8_t* groupStart, const int8_t* groupEnd, std::vector<int>& permutation, size_t indexInPermutation, size_t itemsLeft, const Func& func) {
	if(itemsLeft == 0) {
		return existsPermutationOverVariableGroupsRecursive(groupStart, groupEnd, permutation, indexInPermutation, func);
	} else {
		if(existsSubPermutationRecursive(groupStart, groupEnd, permutation, indexInPermutation + 1, itemsLeft - 1, func)) return true;
		for(size_t i = 1; i < itemsLeft; i++) {
			std::swap(permutation[indexInPermutation], permutation[indexInPermutation + i]);
			if(existsSubPermutationRecursive(groupStart, groupEnd, permutation, indexInPermutation + 1, itemsLeft - 1, func)) return true;
			std::swap(permutation[indexInPermutation], permutation[indexInPermutation + i]);
		}
		return false;
	}
}

template<typename Func>
bool existsPermutationOverVariableGroupsRecursive(const int8_t* start, const int8_t* end, std::vector<int>& permutation, size_t indexInPermutation, const Func& func) {
	if(start != end) {
		int groupSize = 0;
		int8_t groupID = *start;
		do {
			start++;
			groupSize++;
		} while(start != end && *start == groupID);
		return existsSubPermutationRecursive(start, end, permutation, indexInPermutation, groupSize, func);
	} else {
		return func(permutation);
	}
}

template<typename Func>
bool existsPermutationOverVariableGroups(const int8_t* groups, const int8_t* groupsEnd, std::vector<int> permutation, const Func& func) {
	return existsPermutationOverVariableGroupsRecursive(groups, groupsEnd, permutation, 0, func);
}

bool EquivalenceClass::contains(const PreprocessedFunctionInputSet& b) const {
	return this->functionInputSet == b.functionInputSet;
}
