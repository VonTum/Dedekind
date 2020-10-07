#pragma once

#include <cassert>
#include <algorithm>

#include "collectionOperations.h"
#include "functionInput.h"
#include "functionInputSet.h"

struct VariableOccurence {
	int index;
	int count;
};

struct VariableOccurenceGroup {
	int groupSize;
	int occurences;
};
bool operator==(VariableOccurenceGroup a, VariableOccurenceGroup b) { return a.groupSize == b.groupSize && a.occurences == b.occurences; }
bool operator!=(VariableOccurenceGroup a, VariableOccurenceGroup b) { return a.groupSize != b.groupSize || a.occurences != b.occurences; }

struct PreprocessedFunctionInputSet {
	std::vector<FunctionInput> functionInputSet;
	std::vector<VariableOccurenceGroup> variableOccurences;
	int spanSize;

	inline size_t size() const { return functionInputSet.size(); }

	static PreprocessedFunctionInputSet emptyPreprocessedFunctionInputSet;
};

PreprocessedFunctionInputSet PreprocessedFunctionInputSet::emptyPreprocessedFunctionInputSet = PreprocessedFunctionInputSet{std::vector<FunctionInput>{}, std::vector<VariableOccurenceGroup>{}, 0};

std::vector<VariableOccurence> computeOccurenceCounts(const std::vector<FunctionInput>& inputSet, int spanSize) {
	std::vector<VariableOccurence> result(spanSize);


	for(int bit = 0; bit < spanSize; bit++) {
		int total = 0;
		for(FunctionInput f : inputSet) {
			if(f[bit]) {
				total++;
			}
		}
		result[bit] = VariableOccurence{bit, total};
	}

	std::sort(result.begin(), result.end(), [](VariableOccurence a, VariableOccurence b) {return a.count < b.count; });

	return result;
}

PreprocessedFunctionInputSet preprocess(const std::vector<FunctionInput>& inputSet, FunctionInput sp) {
	std::vector<FunctionInput> result(inputSet.size(), FunctionInput{0});

	int outputIndex = 0;
	int inputIndex = 0;
	FunctionInput shiftingSP = sp;
	while(!shiftingSP.empty()) {
		if(shiftingSP[0]) {
			for(size_t i = 0; i < inputSet.size(); i++) {
				if(inputSet[i][inputIndex]) result[i].enableInput(outputIndex);
			}
			outputIndex++;
		}
		shiftingSP >>= 1;
		inputIndex++;
	}
	assert(outputIndex == sp.getNumberEnabled());
	assert(span(result) == FunctionInput{lowerBitsOnes(outputIndex)});
	std::vector<VariableOccurence> occ = computeOccurenceCounts(result, outputIndex);
	std::vector<int> permutation(occ.size());
	for(size_t i = 0; i < occ.size(); i++) { permutation[i] = occ[i].index; }

	std::vector<VariableOccurenceGroup> occurences;
	if(occ.size() > 0) {
		int curOccurenceCount = occ[0].count;
		int occurenceGroupSize = 1;
		for(size_t i = 1; i < occ.size(); i++) {
			VariableOccurence cur = occ[i];
			if(cur.count != curOccurenceCount) {
				// new group
				occurences.push_back(VariableOccurenceGroup{occurenceGroupSize, curOccurenceCount});
				curOccurenceCount = cur.count;
				occurenceGroupSize = 1;
			} else {
				occurenceGroupSize++;
			}
		}
		occurences.push_back(VariableOccurenceGroup{occurenceGroupSize, curOccurenceCount});
	}

	swizzleVector(permutation, result, result);
	return PreprocessedFunctionInputSet{result, occurences, outputIndex};
}

PreprocessedFunctionInputSet preprocess(const std::vector<FunctionInput>& inputSet) {
	FunctionInput sp = span(inputSet);

	return preprocess(inputSet, sp);
}

template<typename Func>
bool existsSubPermutationRecursive(std::vector<VariableOccurenceGroup>::const_iterator groupStart, std::vector<VariableOccurenceGroup>::const_iterator groupEnd, std::vector<int>& permutation, size_t indexInPermutation, size_t itemsLeft, const Func& func) {
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
bool existsPermutationOverVariableGroupsRecursive(std::vector<VariableOccurenceGroup>::const_iterator start, std::vector<VariableOccurenceGroup>::const_iterator end, std::vector<int>& permutation, size_t indexInPermutation, const Func& func) {
	if(start != end) {
		return existsSubPermutationRecursive(start + 1, end, permutation, indexInPermutation, (*start).groupSize, func);
	} else {
		return func(permutation);
	}
}

template<typename Func>
bool existsPermutationOverVariableGroups(const std::vector<VariableOccurenceGroup>& groups, std::vector<int> permutation, const Func& func) {
	return existsPermutationOverVariableGroupsRecursive(groups.begin(), groups.end(), permutation, 0, func);
}


uint32_t hashInputSet(const std::vector<FunctionInput>& inputSet) {
	uint32_t result = 0;
	for(const FunctionInput& f : inputSet) {
		result += f.inputBits;
	}
	return result;
}

struct EquivalenceClass : public PreprocessedFunctionInputSet {
	uint32_t hash;

	EquivalenceClass() = default;
	EquivalenceClass(const PreprocessedFunctionInputSet& parent, int32_t hash) : PreprocessedFunctionInputSet(parent), hash(hash) {}
	EquivalenceClass(PreprocessedFunctionInputSet&& parent, int32_t hash) : PreprocessedFunctionInputSet(std::move(parent)), hash(hash) {}

	EquivalenceClass(const PreprocessedFunctionInputSet& parent) : PreprocessedFunctionInputSet(parent), hash(hashInputSet(parent.functionInputSet)) {}
	EquivalenceClass(PreprocessedFunctionInputSet&& parent) : PreprocessedFunctionInputSet(parent), hash(hashInputSet(parent.functionInputSet)) {}

	static EquivalenceClass emptyEquivalenceClass;

	inline bool contains(const PreprocessedFunctionInputSet& b) const {
		assert(this->size() == b.size()); // assume we are only comparing functionInputSets with the same number of edges

		if(this->spanSize != b.spanSize || this->variableOccurences != b.variableOccurences) return false; // early exit, a and b do not span the same number of variables, impossible to be isomorphic!

		std::vector<FunctionInput> permutedB(b.functionInputSet.size()); // avoid frequent alloc/dealloc
		return existsPermutationOverVariableGroups(this->variableOccurences, generateIntegers(spanSize), [this, &bp = b.functionInputSet, &permutedB](const std::vector<int>& permutation) {
			swizzleVector(permutation, bp, permutedB);
			uint32_t bHash = hashInputSet(permutedB);
			if(this->hash != bHash) {
				return false;
			}
			const std::vector<FunctionInput>& aInput = this->functionInputSet;
			return unordered_contains_all(aInput.begin(), aInput.end(), permutedB.begin(), permutedB.end());
		});
	}
};

EquivalenceClass EquivalenceClass::emptyEquivalenceClass = EquivalenceClass(PreprocessedFunctionInputSet::emptyPreprocessedFunctionInputSet, 0);

bool isIsomorphic(const PreprocessedFunctionInputSet& a, const PreprocessedFunctionInputSet& b) {
	assert(a.size() == b.size()); // assume we are only comparing functionInputSets with the same number of edges

	if(a.spanSize != b.spanSize) return false; // early exit, a and b do not span the same number of variables, impossible to be isomorphic!

	return EquivalenceClass(a).contains(b);
}

bool isIsomorphic(const PreprocessedFunctionInputSet& a, const std::vector<FunctionInput>& b) {
	assert(a.size() == b.size()); // assume we are only comparing functionInputSets with the same number of edges

	FunctionInput spb = span(b);

	if(a.spanSize != spb.getNumberEnabled()) return false; // early exit, a and b do not span the same number of variables, impossible to be isomorphic!

	PreprocessedFunctionInputSet bn = preprocess(b, spb);

	return isIsomorphic(a, bn);
}

bool isIsomorphic(const std::vector<FunctionInput>& a, const std::vector<FunctionInput>& b) {
	assert(a.size() == b.size()); // assume we are only comparing functionInputSets with the same number of edges

	FunctionInput spa = span(a);
	FunctionInput spb = span(b);

	if(spa.getNumberEnabled() != spb.getNumberEnabled()) return false; // early exit, a and b do not span the same number of variables, impossible to be isomorphic!

	PreprocessedFunctionInputSet an = preprocess(a, spa);

	return isIsomorphic(an, b);
}

