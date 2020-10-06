
#include <vector>
#include <intrin.h>
#include <cassert>
#include <ostream>
#include <algorithm>

#include "collectionOperations.h"

uint32_t lowerBitsOnes(int n) {
	return (1U << n) - 1;
}
uint32_t oneBitInterval(int n, int offset) {
	return lowerBitsOnes(n) << offset;
}

struct FunctionInput {
	uint32_t inputBits;

	bool operator[](int index) const {
		return (inputBits & (1 << index)) != 0;
	}

	void enableInput(int index) {
		inputBits |= (1 << index);
	}
	void disableInput(int index) {
		inputBits &= !(1 << index);
	}
	int getNumberEnabled() const {
		return __popcnt(inputBits);
	}
	bool empty() const {
		return inputBits == 0;
	}
	FunctionInput swizzle(const std::vector<int>& swiz) const {
		FunctionInput result{0};

		for(int i = 0; i < swiz.size(); i++) {
			if((*this)[swiz[i]]) {
				result.enableInput(i);
			}
		}

		return result;
	}

	FunctionInput& operator&=(FunctionInput f) { this->inputBits &= f.inputBits; return *this; }
	FunctionInput& operator|=(FunctionInput f) { this->inputBits |= f.inputBits; return *this; }
	FunctionInput& operator^=(FunctionInput f) { this->inputBits ^= f.inputBits; return *this; }
	FunctionInput& operator>>=(size_t shift) { this->inputBits >>= shift; return *this; }
	FunctionInput& operator<<=(size_t shift) { this->inputBits <<= shift; return *this; }
};

FunctionInput operator&(FunctionInput a, FunctionInput b) { return FunctionInput{a.inputBits & b.inputBits}; }
FunctionInput operator|(FunctionInput a, FunctionInput b) { return FunctionInput{a.inputBits | b.inputBits}; }
FunctionInput operator^(FunctionInput a, FunctionInput b) { return FunctionInput{a.inputBits ^ b.inputBits}; }
FunctionInput operator>>(FunctionInput a, size_t shift) { return FunctionInput{a.inputBits >> shift}; }
FunctionInput operator<<(FunctionInput a, size_t shift) { return FunctionInput{a.inputBits << shift}; }

bool operator==(FunctionInput a, FunctionInput b) {
	return a.inputBits == b.inputBits;
}
bool operator!=(FunctionInput a, FunctionInput b) {
	return a.inputBits != b.inputBits;
}
// returns true if all true inputs in a are also true in b
bool isSubSet(FunctionInput a, FunctionInput b) {
	return (a.inputBits & b.inputBits) == a.inputBits;
}
bool overlaps(FunctionInput a, FunctionInput b) {
	return (a.inputBits & b.inputBits) != 0;
}

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
};

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
	for(size_t i = 0; i < occ.size(); i++) {permutation[i] = occ[i].index;}
	
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

	/*std::vector<int> occurences(occ.size());
	for(size_t i = 0; i < occ.size(); i++) { occurences[i] = occ[i].count; }*/

	swizzleVector(permutation, result, result);
	return PreprocessedFunctionInputSet{result, occurences, outputIndex};
}

PreprocessedFunctionInputSet preprocess(const std::vector<FunctionInput>& inputSet) {
	FunctionInput sp = span(inputSet);

	return preprocess(inputSet, sp);
}

/*std::vector<std::vector<FunctionInput>> splitDisjunct(const std::vector<FunctionInput>& inputSet) {
	std::vector<std::vector<FunctionInput>> result;

	for(FunctionInput f : inputSet) {
		for(std::vector<FunctionInput>& group : result) {
			for(FunctionInput groupItem : group) {
				if(overlaps(f, groupItem)) {
					group.push_back(f);
					goto next;
				}
			}
		}

		next:;
	}
}*/

struct HashedNormalizedFunctionInputSet : public PreprocessedFunctionInputSet {
	uint32_t hash;

	HashedNormalizedFunctionInputSet() = default;
	HashedNormalizedFunctionInputSet(const PreprocessedFunctionInputSet& parent, uint32_t hash) : PreprocessedFunctionInputSet(parent), hash(hash) {}
	HashedNormalizedFunctionInputSet(PreprocessedFunctionInputSet&& parent, uint32_t hash) : PreprocessedFunctionInputSet(std::move(parent)), hash(hash) {}
};

uint32_t hash(const std::vector<FunctionInput>& inputSet) {
	uint32_t result = 0;
	for(const FunctionInput& f : inputSet) {
		result += f.inputBits;
	}
	return result;
}

HashedNormalizedFunctionInputSet hash(PreprocessedFunctionInputSet&& set) {
	uint32_t h = hash(set.functionInputSet);
	return HashedNormalizedFunctionInputSet(std::move(set), h);
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

bool isIsomorphic(const HashedNormalizedFunctionInputSet& a, const PreprocessedFunctionInputSet& b) {
	assert(a.size() == b.size()); // assume we are only comparing functionInputSets with the same number of edges

	if(a.spanSize != b.spanSize) return false; // early exit, a and b do not span the same number of variables, impossible to be isomorphic!

	int spanSize = a.spanSize;

	if(a.variableOccurences != b.variableOccurences) {
		return false;
	}

	std::vector<FunctionInput> permutedB(b.functionInputSet.size()); // avoid frequent alloc/dealloc
	return existsPermutationOverVariableGroups(a.variableOccurences, generateIntegers(spanSize), [&a, &bp = b.functionInputSet, &permutedB](const std::vector<int>& permutation) {
		swizzleVector(permutation, bp, permutedB);
		uint32_t bHash = hash(permutedB);
		if(a.hash != bHash) {
			return false;
		}
		const std::vector<FunctionInput>& aInput = a.functionInputSet;
		return unordered_contains_all(aInput.begin(), aInput.end(), permutedB.begin(), permutedB.end());
	});
}

bool isIsomorphic(const PreprocessedFunctionInputSet& a, const PreprocessedFunctionInputSet& b) {
	assert(a.size() == b.size()); // assume we are only comparing functionInputSets with the same number of edges

	if(a.spanSize != b.spanSize) return false; // early exit, a and b do not span the same number of variables, impossible to be isomorphic!

	return isIsomorphic(HashedNormalizedFunctionInputSet(a, hash(a.functionInputSet)), b);
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

struct SymmetryGroup {
	int64_t count;
	HashedNormalizedFunctionInputSet example;
};

std::vector<SymmetryGroup> findSymmetryGroups(const std::vector<FunctionInput>& available, size_t groupSize) {
	std::vector<SymmetryGroup> foundGroups;

	forEachSubgroup(available, groupSize, [&foundGroups](const std::vector<FunctionInput>& subGroup) {
		PreprocessedFunctionInputSet normalized = preprocess(subGroup);
		for(SymmetryGroup& s : foundGroups) {
			if(isIsomorphic(s.example, normalized)) {
				s.count++;
				return; // don't add new symmetryGroup
			}
		}
		// not found => add new symmetryGroup
		foundGroups.push_back(SymmetryGroup{1, HashedNormalizedFunctionInputSet{normalized, hash(normalized.functionInputSet)}});
	});

	return foundGroups;
}



std::ostream& operator<<(std::ostream& os, FunctionInput f) {
	if(f.empty()) {
		os << '/';
	} else {
		int curIndex = 0;
		int32_t v = f.inputBits;
		while(v != 0) {
			if(v & 0x1) {
				os << char('a' + curIndex);
			}
			v >>= 1;
			curIndex++;
		}
	}
	return os;
}

std::ostream& operator<<(std::ostream& os, VariableOccurence vo) {
	os << char('a' + vo.index) << ':' << vo.count;
	return os;
}
std::ostream& operator<<(std::ostream& os, VariableOccurenceGroup vog) {
	os << vog.groupSize << 'x' << vog.occurences;
	return os;
}
std::ostream& operator<<(std::ostream& os, const PreprocessedFunctionInputSet& s) {
	os << s.functionInputSet;
	/*os << '.';
	for(auto occ : s.variableOccurences) {
		os << occ;
	}*/
	return os;
}

std::ostream& operator<<(std::ostream& os, const SymmetryGroup& s) {
	os << s.example << ':' << s.count;
	return os;
}
