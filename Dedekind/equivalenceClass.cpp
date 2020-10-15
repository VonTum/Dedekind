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

template<typename Iter>
static __m256i parallelSwizzle(__m256i functionInputs, Iter swizStart, Iter swizEnd) {
	__m256i one = _mm256_set1_epi32(1);
	__m256i result = _mm256_setzero_si256();
	int index = 0;
	for(; swizStart != swizEnd; ++swizStart) {
		result = grabBitIntoBit(functionInputs, *swizStart, index, result);
		index++;
	}
	return result;
}



struct SmallEqualityChecker {
	const int_set<FunctionInput, uint32_t>& equalityChecker;
	std::vector<VariableOccurenceGroup>::const_iterator groupIter;
	std::vector<VariableOccurenceGroup>::const_iterator groupsEnd;
	size_t functionInputSetSize;
	__m256i data;
	__m256i curMask = _mm256_set1_epi32(1);
	__m256i bits1 = _mm256_undefined_si256();
	__m256i bits2 = _mm256_undefined_si256();
	__m256i bits3 = _mm256_undefined_si256();
	__m256i bits4 = _mm256_undefined_si256();
	__m256i bits5 = _mm256_undefined_si256();
	__m256i bits6 = _mm256_undefined_si256();
	__m256i bits7 = _mm256_undefined_si256();
	__m256i bits8 = _mm256_undefined_si256();
	__m256i bits9 = _mm256_undefined_si256();
	__m256i curValue = _mm256_setzero_si256();
	static_assert(MAX_DEDEKIND <= 10, "number of bits registers should be extended for higher order equivalence checking");

	SmallEqualityChecker(const int_set<FunctionInput, uint32_t>& equalityChecker, const aligned_set<FunctionInput, 32>& functionInputSet, const std::vector<VariableOccurenceGroup>& occurenceGroups) :
		equalityChecker(equalityChecker), 
		groupIter(occurenceGroups.begin()),
		groupsEnd(occurenceGroups.end()),
		functionInputSetSize(functionInputSet.size()), 
		data(_mm256_load_si256(reinterpret_cast<const __m256i*>(functionInputSet.getData()))) {}

	inline bool allContained() {
		for(int i = 0; i < functionInputSetSize; i++) {
			FunctionInput fn{curValue.m256i_i32[i]};
			if(!equalityChecker.contains(fn)) {
				return false;
			}
		}
		return true;
	}

	void checkAllForGroup();

	inline void nextGroup() {
		if(groupIter != groupsEnd) {
			checkAllForGroup();
		} else {
			allContained();
		}
	}

	inline void permute1() {
		curValue = _mm256_or_si256(curValue, bits1);
		nextGroup();
		curValue = _mm256_andnot_si256(curValue, bits1);
	}
	inline void permute2() {
		
	}
	inline void permute3() {}
	inline void permute4() {}
	inline void permute5() {
		
	}
	inline void permute6() {}
	inline void permute7() {}
	inline void permute8() {}
	inline void permute9() {}
};

void SmallEqualityChecker::checkAllForGroup() {
	int curGroupSize = (*groupIter).groupSize;
	++groupIter;
	switch(curGroupSize) {
	case 1:
		bits9 = bits8; bits8 = bits7; bits7 = bits6; bits6 = bits5; bits5 = bits4; bits4 = bits3; bits3 = bits2; bits2 = bits1;
		bits1 = _mm256_and_si256(data, curMask);
		curMask = _mm256_slli_epi32(curMask, 1);
		permute1();
		curMask = _mm256_srli_epi32(curMask, 1);
		bits1 = bits2; bits2 = bits3; bits3 = bits4; bits4 = bits5; bits5 = bits6; bits6 = bits7; bits7 = bits8; bits8 = bits9;
		break;
	case 2:
		bits9 = bits7; bits8 = bits6; bits7 = bits5; bits6 = bits4; bits5 = bits3; bits4 = bits2; bits3 = bits1;
		bits1 = _mm256_and_si256(data, curMask);
		bits2 = _mm256_and_si256(_mm256_srli_epi32(data, 2), curMask);
		curMask = _mm256_slli_epi32(curMask, 2);
		permute2();
		curMask = _mm256_srli_epi32(curMask, 2);
		bits1 = bits3; bits2 = bits4; bits3 = bits5; bits4 = bits6; bits5 = bits7; bits6 = bits8; bits7 = bits9;
		break;
	case 3:
		bits9 = bits6; bits8 = bits5; bits7 = bits4; bits6 = bits3; bits5 = bits2; bits4 = bits1;
		bits1 = _mm256_and_si256(data, curMask);
		bits2 = _mm256_and_si256(_mm256_srli_epi32(data, 2), curMask);
		bits3 = _mm256_and_si256(_mm256_srli_epi32(data, 3), curMask);
		curMask = _mm256_slli_epi32(curMask, 3);
		permute3();
		curMask = _mm256_srli_epi32(curMask, 3);
		bits1 = bits4; bits2 = bits5; bits3 = bits6; bits4 = bits7; bits5 = bits8; bits6 = bits9;
		break;
	case 4:
		bits9 = bits5; bits8 = bits4; bits7 = bits3; bits6 = bits2; bits5 = bits1;
		bits1 = _mm256_and_si256(data, curMask);
		bits2 = _mm256_and_si256(_mm256_srli_epi32(data, 2), curMask);
		bits3 = _mm256_and_si256(_mm256_srli_epi32(data, 3), curMask);
		bits4 = _mm256_and_si256(_mm256_srli_epi32(data, 4), curMask);
		curMask = _mm256_slli_epi32(curMask, 4);
		permute4();
		curMask = _mm256_srli_epi32(curMask, 4);
		bits1 = bits5; bits2 = bits6; bits3 = bits7; bits4 = bits8; bits5 = bits9;
		break;
	case 5:
		bits9 = bits4; bits8 = bits3; bits7 = bits2; bits6 = bits1;
		bits1 = _mm256_and_si256(data, curMask);
		bits2 = _mm256_and_si256(_mm256_srli_epi32(data, 2), curMask);
		bits3 = _mm256_and_si256(_mm256_srli_epi32(data, 3), curMask);
		bits4 = _mm256_and_si256(_mm256_srli_epi32(data, 4), curMask);
		bits5 = _mm256_and_si256(_mm256_srli_epi32(data, 5), curMask);
		curMask = _mm256_slli_epi32(curMask, 5);
		permute5();
		curMask = _mm256_srli_epi32(curMask, 5);
		bits1 = bits6; bits2 = bits7; bits3 = bits8; bits4 = bits9;
		break;
	case 6:
		bits9 = bits3; bits8 = bits2; bits7 = bits1;
		bits1 = _mm256_and_si256(data, curMask);
		bits2 = _mm256_and_si256(_mm256_srli_epi32(data, 2), curMask);
		bits3 = _mm256_and_si256(_mm256_srli_epi32(data, 3), curMask);
		bits4 = _mm256_and_si256(_mm256_srli_epi32(data, 4), curMask);
		bits5 = _mm256_and_si256(_mm256_srli_epi32(data, 5), curMask);
		bits6 = _mm256_and_si256(_mm256_srli_epi32(data, 6), curMask);
		curMask = _mm256_slli_epi32(curMask, 6);
		permute6();
		curMask = _mm256_srli_epi32(curMask, 6);
		bits1 = bits7; bits2 = bits8; bits3 = bits9;
		break;
	case 7:
		bits9 = bits2; bits8 = bits1;
		bits1 = _mm256_and_si256(data, curMask);
		bits2 = _mm256_and_si256(_mm256_srli_epi32(data, 2), curMask);
		bits3 = _mm256_and_si256(_mm256_srli_epi32(data, 3), curMask);
		bits4 = _mm256_and_si256(_mm256_srli_epi32(data, 4), curMask);
		bits5 = _mm256_and_si256(_mm256_srli_epi32(data, 5), curMask);
		bits6 = _mm256_and_si256(_mm256_srli_epi32(data, 6), curMask);
		bits7 = _mm256_and_si256(_mm256_srli_epi32(data, 7), curMask);
		curMask = _mm256_slli_epi32(curMask, 7);
		permute7();
		curMask = _mm256_srli_epi32(curMask, 7);
		bits1 = bits8; bits2 = bits9;
		break;
	case 8:
		bits9 = bits1;
		bits1 = _mm256_and_si256(data, curMask);
		bits2 = _mm256_and_si256(_mm256_srli_epi32(data, 2), curMask);
		bits3 = _mm256_and_si256(_mm256_srli_epi32(data, 3), curMask);
		bits4 = _mm256_and_si256(_mm256_srli_epi32(data, 4), curMask);
		bits5 = _mm256_and_si256(_mm256_srli_epi32(data, 5), curMask);
		bits6 = _mm256_and_si256(_mm256_srli_epi32(data, 6), curMask);
		bits7 = _mm256_and_si256(_mm256_srli_epi32(data, 7), curMask);
		bits8 = _mm256_and_si256(_mm256_srli_epi32(data, 8), curMask);
		curMask = _mm256_slli_epi32(curMask, 8);
		permute8();
		curMask = _mm256_srli_epi32(curMask, 8);
		bits1 = bits9;
		break;
	case 9:

		bits1 = _mm256_and_si256(data, curMask);
		bits2 = _mm256_and_si256(_mm256_srli_epi32(data, 2), curMask);
		bits3 = _mm256_and_si256(_mm256_srli_epi32(data, 3), curMask);
		bits4 = _mm256_and_si256(_mm256_srli_epi32(data, 4), curMask);
		bits5 = _mm256_and_si256(_mm256_srli_epi32(data, 5), curMask);
		bits6 = _mm256_and_si256(_mm256_srli_epi32(data, 6), curMask);
		bits7 = _mm256_and_si256(_mm256_srli_epi32(data, 7), curMask);
		bits8 = _mm256_and_si256(_mm256_srli_epi32(data, 8), curMask);
		bits9 = _mm256_and_si256(_mm256_srli_epi32(data, 9), curMask);
		curMask = _mm256_slli_epi32(curMask, 9);
		permute9();
		curMask = _mm256_srli_epi32(curMask, 9);

		break;


	}
	--groupIter;
}

template<typename Func>
bool existsSubPermutationSIMDRecursive(vIter groupStart, vIter groupEnd, __m256i curVal, int indexInPermutation, int* itemsLeftToPlace, int numberOfItemsLeftToPlace, __m256i data, const Func& func) {
	if(numberOfItemsLeftToPlace == 0) {
		groupStart++;
		if(groupStart != groupEnd) {
			return existsSubPermutationSIMDRecursive(groupStart, groupEnd, curVal, indexInPermutation, itemsLeftToPlace, (*groupStart).groupSize, data, func);
		} else {
			return func(curVal, itemsLeftToPlace - indexInPermutation, itemsLeftToPlace);
		}
	} else {
		for(int i = 0;;) {
			__m256i newVal = grabBitIntoBit(data, itemsLeftToPlace[0], indexInPermutation, curVal);
			if(existsSubPermutationSIMDRecursive(groupStart, groupEnd, newVal, indexInPermutation + 1, itemsLeftToPlace+1, numberOfItemsLeftToPlace-1, data, func)) return true;
			i++;
			if(!(i < numberOfItemsLeftToPlace)) break;
			std::swap(itemsLeftToPlace[0], itemsLeftToPlace[i]);
		}
		int firstVal = itemsLeftToPlace[0];
		for(int i = 1; i < numberOfItemsLeftToPlace; i++) {
			itemsLeftToPlace[i - 1] = itemsLeftToPlace[i];
		}
		itemsLeftToPlace[numberOfItemsLeftToPlace - 1] = firstVal;
		return false;
	}
}

template<typename Func>
bool existsPermutationOverVariableGroupsSIMD(int numberOfVariables, const std::vector<VariableOccurenceGroup>& groups, __m256i data, const Func& func) {
	__m256i startingVal = _mm256_setzero_si256();

	int itemsLeftToPlace[MAX_DEDEKIND];
	for(int i = 0; i < numberOfVariables; i++) {
		itemsLeftToPlace[i] = i;
	}
	return existsSubPermutationSIMDRecursive(groups.begin(), groups.end(), startingVal, 0, itemsLeftToPlace, (*groups.begin()).groupSize, data, func);
}

bool EquivalenceClass::contains(const PreprocessedFunctionInputSet& b) const {
	assert(this->size() == b.size()); // assume we are only comparing functionInputSets with the same number of edges

	if(this->spanSize != b.spanSize || this->variableOccurences != b.variableOccurences) return false; // early exit, a and b do not span the same number of variables, impossible to be isomorphic!

	if(b.functionInputSet.size() <= 8) {
		//SmallEqualityChecker eqChecker(this->equalityChecker, b.functionInputSet);


		__m256i initialData = _mm256_load_si256(reinterpret_cast<const __m256i*>(b.functionInputSet.getData()));
		return existsPermutationOverVariableGroupsSIMD(this->spanSize, this->variableOccurences, initialData, [&eqCheck = this->equalityChecker, bsize = b.functionInputSet.size()](__m256i swizzledFirstBlock, int* swizStart, int* swizEnd){
			for(int i = 0; i < bsize; i++) {
				FunctionInput fn{swizzledFirstBlock.m256i_i32[i]};
				if(!eqCheck.contains(fn)) {
					return false;
				}
			}
			return true;
		});
		return false;
	} else {
		__m256i initialData = _mm256_load_si256(reinterpret_cast<const __m256i*>(b.functionInputSet.getData()));
		return existsPermutationOverVariableGroupsSIMD(this->spanSize, this->variableOccurences, initialData, [this, &bp = b.functionInputSet](__m256i swizzledFirstBlock, int* swizStart, int* swizEnd) {
			for(int i = 0; i < 8; i++) {
				FunctionInput fn{swizzledFirstBlock.m256i_i32[i]};
				if(!equalityChecker.contains(fn)) {
					return false;
				}
			}
			
			size_t blockCount = bp.size() / 8;
			for(size_t i = 1; i < blockCount; i++) {
				__m256i curBlock = _mm256_load_si256(reinterpret_cast<const __m256i*>(bp.getData() + i * 8));
				__m256i swizzledBlock = parallelSwizzle(curBlock, swizStart, swizEnd);
				for(int i = 0; i < 8; i++) {
					if(!equalityChecker.contains(FunctionInput{swizzledBlock.m256i_u32[i]})) {
						return false;
					}
				}
			}
			if(bp.size() % 8 != 0) {
				__m256i curBlock = _mm256_load_si256(reinterpret_cast<const __m256i*>(bp.getData() + blockCount * 8));
				__m256i swizzledBlock = parallelSwizzle(curBlock, swizStart, swizEnd);
				for(int i = 0; i < bp.size() - blockCount * 8; i++) {
					if(!equalityChecker.contains(FunctionInput{swizzledBlock.m256i_u32[i]})) {
						return false;
					}
				}
			}
			return true;
		});
	}

	/*std::vector<int> permut = generateIntegers(spanSize);
	return existsPermutationOverVariableGroups(this->variableOccurences, std::move(permut), [this, &bp = b.functionInputSet](const std::vector<int>& permutation) {
		for(FunctionInput fn : bp) {
			if(!equalityChecker.contains(fn.swizzle(permutation))) return false;
		}
		return true;
	});*/
}
