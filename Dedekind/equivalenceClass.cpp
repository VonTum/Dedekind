#include "equivalenceClass.h"

#include <immintrin.h>


PreprocessedFunctionInputSet PreprocessedFunctionInputSet::emptyPreprocessedFunctionInputSet = PreprocessedFunctionInputSet{FunctionInputSet{}, std::vector<VariableOccurenceGroup>{}, 0};
EquivalenceClass EquivalenceClass::emptyEquivalenceClass = EquivalenceClass(PreprocessedFunctionInputSet::emptyPreprocessedFunctionInputSet);


static std::vector<VariableOccurence> computeOccurenceCounts(const FunctionInputSet& inputSet, int spanSize) {
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

PreprocessedFunctionInputSet preprocess(const FunctionInputSet& inputSet, FunctionInput sp) {
	FunctionInputSet result(inputSet.size(), FunctionInput{0});

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
	assert(span(result) == FunctionInput::allOnes(outputIndex));
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

typedef std::vector<VariableOccurenceGroup>::const_iterator vIter;

template<typename Func>
bool existsSubPermutationRecursive(vIter groupStart, vIter groupEnd, std::vector<int>& permutation, size_t indexInPermutation, size_t itemsLeft, const Func& func) {
	if(itemsLeft == 0) {
		return existsPermutationOverVariableGroupsRecursive(groupStart + 1, groupEnd, permutation, indexInPermutation, func);
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
bool existsPermutationOverVariableGroupsRecursive(vIter start, vIter end, std::vector<int>& permutation, size_t indexInPermutation, const Func& func) {
	if(start != end) {
		return existsSubPermutationRecursive(start, end, permutation, indexInPermutation, (*start).groupSize, func);
	} else {
		return func(permutation);
	}
}

template<typename Func>
bool existsPermutationOverVariableGroups(const std::vector<VariableOccurenceGroup>& groups, std::vector<int> permutation, const Func& func) {
	return existsPermutationOverVariableGroupsRecursive(groups.begin(), groups.end(), permutation, 0, func);
}

static const __m256i one = _mm256_set1_epi32(1);
static __m256i makeMask(size_t offset) {
	return _mm256_sll_epi32(one, _mm_set1_epi64x(offset));
}

static __m256i grabBitIntoBit(__m256i data, long long bitToGrab, long long bitToStoreIn, __m256i storeIn) {
	__m256i one = _mm256_set1_epi32(1);

	__m128i grabOffset = _mm_set1_epi64x(bitToGrab);
	__m128i storeOffset = _mm_set1_epi64x(bitToStoreIn);

	__m256i maskToGrab = _mm256_sll_epi32(one, grabOffset);
	__m256i maskedGrabbed = _mm256_and_si256(data, maskToGrab);
	__m256i shiftedToFirstBit = _mm256_srl_epi32(maskedGrabbed, grabOffset);
	__m256i outputShifted = _mm256_sll_epi32(shiftedToFirstBit, storeOffset);

	return _mm256_or_si256(storeIn, outputShifted);
}

/*
	
*/

template<typename Func>
bool existsSubPermutationSIMDRecursive(vIter groupStart, vIter groupEnd, __m256i curVal, size_t indexInPermutation, int* itemsLeftToPlace, size_t numberOfItemsLeftToPlace, __m256i data, const Func& func) {
	if(numberOfItemsLeftToPlace == 0) {
		return existsPermutationOverVariableGroupsSIMDRecursive(groupStart + 1, groupEnd, curVal, indexInPermutation, data, func);
	} else {
		for(size_t i = 0;;) {
			__m256i newVal = grabBitIntoBit(data, itemsLeftToPlace[0], indexInPermutation, curVal);
			if(existsSubPermutationSIMDRecursive(groupStart, groupEnd, newVal, indexInPermutation + 1, itemsLeftToPlace+1, numberOfItemsLeftToPlace-1, data, func)) return true;
			i++;
			if(!(i < numberOfItemsLeftToPlace)) break;
			std::swap(itemsLeftToPlace[0], itemsLeftToPlace[i]);
			// 0 1 2 3
			// 1 0 2 3
			// 2 0 1 3
			// 3 0 1 2
		}
		int firstVal = itemsLeftToPlace[0];
		for(size_t i = 1; i < numberOfItemsLeftToPlace; i++) {
			itemsLeftToPlace[i - 1] = itemsLeftToPlace[i];
		}
		itemsLeftToPlace[numberOfItemsLeftToPlace - 1] = firstVal;
		return false;
	}
}

template<typename Func>
bool existsPermutationOverVariableGroupsSIMDRecursive(vIter start, vIter end, __m256i curVal, size_t indexInPermutation, __m256i data, const Func& func) {
	if(start != end) {
		size_t groupSize = (*start).groupSize;
		int* group = new int[groupSize];
		for(int i = 0; i < groupSize; i++) {
			group[i] = i + indexInPermutation;
		}
		return existsSubPermutationSIMDRecursive(start, end, curVal, indexInPermutation, group, groupSize, data, func);
		delete[] group;
	} else {
		return func(curVal);
	}
}

template<typename Func>
bool existsPermutationOverVariableGroupsSIMD(const std::vector<VariableOccurenceGroup>& groups, __m256i data, const Func& func) {
	__m256i startingVal = _mm256_setzero_si256();
	return existsPermutationOverVariableGroupsSIMDRecursive(groups.begin(), groups.end(), startingVal, 0, data, func);
}

bool EquivalenceClass::contains(const PreprocessedFunctionInputSet& b) const {
	assert(this->size() == b.size()); // assume we are only comparing functionInputSets with the same number of edges

	if(this->spanSize != b.spanSize || this->variableOccurences != b.variableOccurences) return false; // early exit, a and b do not span the same number of variables, impossible to be isomorphic!

	__m256i initialData = _mm256_load_si256(reinterpret_cast<const __m256i*>(b.functionInputSet.getData()));
	if(b.functionInputSet.size() <= 8) {
		return existsPermutationOverVariableGroupsSIMD(this->variableOccurences, initialData, [this, sz = b.functionInputSet.size()](__m256i swizzled){
			for(int i = 0; i < sz; i++) {
				FunctionInput fn{swizzled.m256i_i32[i]};
				if(!equalityChecker.contains(fn)) {
					return false;
				}
			}
			return true;
		});
		return false;
	} else {
		std::vector<int> permut = generateIntegers(spanSize);
		return existsPermutationOverVariableGroups(this->variableOccurences, std::move(permut), [this, &bp = b.functionInputSet](const std::vector<int>& permutation) {
			for(FunctionInput bIn : bp) {
				if(!equalityChecker.contains(bIn.swizzle(permutation))) {
					return false;
				}
			}
			return true;
		});
	}
}
