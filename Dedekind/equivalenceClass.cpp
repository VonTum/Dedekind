#include "equivalenceClass.h"

#include <immintrin.h>

#include <iostream>


PreprocessedFunctionInputSet PreprocessedFunctionInputSet::emptyPreprocessedFunctionInputSet = PreprocessedFunctionInputSet{FunctionInputSet{}, std::vector<VariableGroup>{}, 0};
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

static std::vector<VariableCoOccurence> computeCoOccurences(const std::vector<int>& groupAssignments, const FunctionInputSet& functionInputSet) {
	int variableCount = groupAssignments.size();

	std::vector<VariableCoOccurence> result(groupAssignments.size());

	for(int curVar = 0; curVar < variableCount; curVar++) {
		std::vector<std::vector<int>> curVarCoOccurence;
		for(FunctionInput fi : functionInputSet) {
			if(fi[curVar]) {
				std::vector<int> coOccurrence;
				for(int foundVar : fi) {
					if(foundVar == curVar) continue;
					coOccurrence.push_back(groupAssignments[foundVar]);
				}
				curVarCoOccurence.push_back(std::move(coOccurrence));
			}
		}

		std::sort(curVarCoOccurence.begin(), curVarCoOccurence.end(), [](const std::vector<int>& first, const std::vector<int>& second) {
			assert(first.size() == second.size());
			for(int i = 0; i < first.size(); i++) {
				if(first[i] < second[i]) {
					return true;
				} else if(first[i] > second[i]) {
					return false;
				}
			}
			return false;
			// apparently std::sort can try to compare a value with itself -_-
			// throw "All values should be uniquely sortable, duplicate element?";
		});

		result[curVar] = VariableCoOccurence{std::move(curVarCoOccurence)};
	}

	return result;
}

static std::vector<int> assignGroups(const std::vector<VariableCoOccurence>& coOccurences) {
	std::vector<int> result(coOccurences.size());

	std::vector<const VariableCoOccurence*> foundGroups;

	for(int curVar = 0; curVar < coOccurences.size(); curVar++) {
		for(int foundGroupIndex = 0; foundGroupIndex < foundGroups.size(); foundGroupIndex++) {
			if(coOccurences[curVar].coOccursWith == foundGroups[foundGroupIndex]->coOccursWith) {
				result[curVar] = foundGroupIndex;
				goto found;
			}
		}
		result[curVar] = foundGroups.size();
		foundGroups.push_back(&coOccurences[curVar]);
		found:;
	}

	return result;
}

static int getHighest(const std::vector<int>& items) {
	int highest = items[0];
	for(int i = 1; i < items.size(); i++) {
		if(items[i] > highest) highest = items[i];
	}
	return highest;
}
static std::pair<std::vector<int>, std::vector<VariableCoOccurence>> refineVariableSymmetryGroups(std::vector<int> groups, const FunctionInputSet& functionInputSet) {
	int oldNumGroups = getHighest(groups);

	while(true) {
		std::vector<VariableCoOccurence> coOccurences = computeCoOccurences(groups, functionInputSet);
		groups = assignGroups(coOccurences);
		int newNumGroups = getHighest(groups);
		if(oldNumGroups == newNumGroups) {// stagnation, all groups found. Number of groups can only go up as groups are further split up
			return std::make_pair(groups, coOccurences);
		}
		oldNumGroups = newNumGroups;
	}
}

struct NormalizeVariablesByOccurenceData {
	FunctionInputSet funcInputSet;
	int variableCount;
	std::vector<VariableOccurence> occurences;
};

// returns the resulting functionInputSet, and the number of found variables
static std::pair<FunctionInputSet, int> preprocessRemoveUnusedVariables(const FunctionInputSet& inputSet) {
	FunctionInputSet resultingFuncInputSet = inputSet;

	int inputIndex = 0;
	FunctionInput shiftingSP = span(inputSet);
	std::vector<int> variableRemap;
	while(!shiftingSP.empty()) {
		if(shiftingSP[0]) {
			variableRemap.push_back(inputIndex);
		}
		shiftingSP >>= 1;
		inputIndex++;
	}
	swizzleVector(variableRemap, resultingFuncInputSet, resultingFuncInputSet);
	int numberOfVariables = variableRemap.size();
	assert(span(resultingFuncInputSet) == FunctionInput::allOnes(numberOfVariables));
	return std::make_pair(resultingFuncInputSet, numberOfVariables);
}

// returns the resulting sorted functionInputSet, and the occurences of each of the variables
static std::pair<FunctionInputSet, std::vector<int>> preprocessSortByOccurences(FunctionInputSet inputSet, int variableCount) {
	std::vector<VariableOccurence> occ = computeOccurenceCounts(inputSet, variableCount);
	std::vector<int> swizzleSources(occ.size());
	std::vector<int> counts(occ.size());
	for(size_t i = 0; i < occ.size(); i++) {
		swizzleSources[i] = occ[i].index;
		counts[i] = occ[i].count;
	}

	swizzleVector(swizzleSources, inputSet, inputSet);
	
	return std::make_pair(std::move(inputSet), std::move(counts));
}

PreprocessedFunctionInputSet preprocess(const FunctionInputSet& inputSet) {
	std::pair<FunctionInputSet, int> variableIndicesNormalized = preprocessRemoveUnusedVariables(inputSet);
	int variableCount = variableIndicesNormalized.second;

	std::pair<FunctionInputSet, std::vector<int>> variablesSortedByOccurence = preprocessSortByOccurences(std::move(variableIndicesNormalized.first), variableCount);
	std::vector<int>& varOccurences = variablesSortedByOccurence.second;

	if(varOccurences.size() != 0) {
		std::vector<VariableGroup> variableGroups{VariableGroup{1, varOccurences[0]}};
		int lastOccurCount = varOccurences[0];
		for(int i = 1; i < varOccurences.size(); i++) {
			int curOccurCount = varOccurences[i];
			if(curOccurCount == lastOccurCount) {
				variableGroups.back().groupSize++;
			} else {
				variableGroups.push_back(VariableGroup{1, curOccurCount});
				lastOccurCount = curOccurCount;
			}
		}
		return PreprocessedFunctionInputSet{variablesSortedByOccurence.first, variableGroups, variableCount};
	}
	return PreprocessedFunctionInputSet{variablesSortedByOccurence.first, std::vector<VariableGroup>{}, variableCount};

	/*std::vector<int> initialGroups(varOccurences.size());
	std::vector<int> knownGroupSizes;
	for(size_t i = 0; i < varOccurences.size(); i++) {
		for(int indexinKnownGroupSizes = 0; indexinKnownGroupSizes < knownGroupSizes.size(); indexinKnownGroupSizes++) {
			if(knownGroupSizes[indexinKnownGroupSizes] == varOccurences[i]) {
				initialGroups[i] = indexinKnownGroupSizes;
				goto nextItem;
			}
		}
		initialGroups[i] = knownGroupSizes.size();
		knownGroupSizes.push_back(varOccurences[i]);
		nextItem:;
	}


	size_t inputSetSize = inputSet.size();
	std::pair<std::vector<int>, std::vector<VariableCoOccurence>> resultingRefinedGroups = refineVariableSymmetryGroups(initialGroups, variablesSortedByOccurence.first);

	std::vector<std::vector<int>> resultingGroupsIndices;
	std::vector<VariableGroup> resultingGroups;
	for(int refinedGroupIndex = 0; refinedGroupIndex < resultingRefinedGroups.second.size(); refinedGroupIndex++) {
		VariableCoOccurence& curCoOccur = resultingRefinedGroups.second[refinedGroupIndex];
		for(int resultingGroupIndex = 0; resultingGroupIndex < resultingGroups.size(); resultingGroupIndex++) {
			VariableGroup& vg = resultingGroups[resultingGroupIndex];
			if(vg.occurences == curCoOccur) {
				vg.groupSize++;
				resultingGroupsIndices[resultingGroupIndex].push_back(refinedGroupIndex);
				goto nextGroup;
			}
		}
		resultingGroups.push_back(VariableGroup{1, curCoOccur});
		resultingGroupsIndices.push_back(std::vector<int>{refinedGroupIndex});
		nextGroup:;
	}
	std::vector<int> resultingSwizzle;
	for(const std::vector<int>& resultingGroupsIndexList : resultingGroupsIndices) {
		for(int groupIndex : resultingGroupsIndexList) {
			resultingSwizzle.push_back(groupIndex);
		}
	}

	swizzleVector(resultingSwizzle, variablesSortedByOccurence.first, variablesSortedByOccurence.first);

	variablesSortedByOccurence.first.fixFinalBlock();*/
	//return PreprocessedFunctionInputSet{variablesSortedByOccurence.first, variableGroups, variableCount};
}

typedef std::vector<VariableGroup>::const_iterator vIter;

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
bool existsPermutationOverVariableGroups(const std::vector<VariableGroup>& groups, std::vector<int> permutation, const Func& func) {
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


#define SWAP(a, b) {__m256i tmp = a; a = b; b = tmp;}
#define ROTRIGHT(a, b, c) {__m256i tmp = c; c = b; b = a; a = tmp;}
struct SmallEqualityChecker {
	const int_set<FunctionInput, uint32_t>& equalityChecker;
	std::vector<VariableGroup>::const_iterator groupIter;
	std::vector<VariableGroup>::const_iterator groupsEnd;
	size_t functionInputSetSize;
	__m256i data;
	__m256i curMask = _mm256_set1_epi32(1);
	__m256i bits0 = _mm256_undefined_si256(); // special value, contains current mask, where ones, value must be destroyed upon exit
	__m256i bits1 = _mm256_undefined_si256();
	__m256i bits2 = _mm256_undefined_si256();
	__m256i bits3 = _mm256_undefined_si256();
	__m256i bits4 = _mm256_undefined_si256();
	__m256i bits5 = _mm256_undefined_si256();
	__m256i bits6 = _mm256_undefined_si256();
	__m256i bits7 = _mm256_undefined_si256();
	__m256i bits8 = _mm256_undefined_si256();
	__m256i bits9 = _mm256_undefined_si256();
	__m256i bits10 = _mm256_undefined_si256();
	__m256i bits11 = _mm256_undefined_si256();
	__m256i bits12 = _mm256_undefined_si256();
	__m256i bits13 = _mm256_undefined_si256();
	__m256i bits14 = _mm256_undefined_si256();
	__m256i bits15 = _mm256_undefined_si256();
	__m256i curValue = _mm256_setzero_si256();
	static_assert(MAX_DEDEKIND <= 10, "number of bits registers should be extended for higher order equivalence checking");

	SmallEqualityChecker(const int_set<FunctionInput, uint32_t>& equalityChecker, const aligned_set<FunctionInput, 32>& functionInputSet, const std::vector<VariableGroup>& occurenceGroups) :
		equalityChecker(equalityChecker), 
		groupIter(occurenceGroups.begin()),
		groupsEnd(occurenceGroups.end()),
		functionInputSetSize(functionInputSet.size()), 
		data(_mm256_load_si256(reinterpret_cast<const __m256i*>(functionInputSet.getData()))) {}

	inline bool allContained() {
		/*for(int i = 0; i < functionInputSetSize; i++) {
			FunctionInput fn{curValue.m256i_i32[i]};
			if(!equalityChecker.contains(fn)) {
				return false;
			}
		}
		return true;*/

		__m256i v = _mm256_i32gather_epi32(reinterpret_cast<const int*>(equalityChecker.getData()), curValue, 1);

		int msk = _mm256_movemask_epi8(v);
		return (msk & 0x11111111) == 0x11111111;
	}

	bool checkAllForGroup();

	inline bool nextGroup() {
		if(groupIter != groupsEnd) {
			return checkAllForGroup();
		} else {
			return allContained();
		}
	}

	inline bool permute1() { // no mask needed to repair permute1, can just reuse bits1
		curValue = _mm256_or_si256(curValue, bits0);
		if(nextGroup()) return true;
		curValue = _mm256_andnot_si256(bits0, curValue);
		return false;
	}
	inline bool permute2() {
		curValue = _mm256_or_si256(curValue, _mm256_or_si256(bits1, _mm256_srli_epi32(bits2, 1)));
		if(nextGroup()) return true;
		curValue = _mm256_andnot_si256(bits0, curValue);
		curValue = _mm256_or_si256(curValue, _mm256_or_si256(bits2, _mm256_srli_epi32(bits1, 1)));
		if(nextGroup()) return true;
		curValue = _mm256_andnot_si256(bits0, curValue);
		return false;
	}
	inline bool permute3() {
		curValue = _mm256_or_si256(curValue, _mm256_or_si256(bits1, _mm256_or_si256(_mm256_srli_epi32(bits2, 1), _mm256_srli_epi32(bits3, 2))));
		if(nextGroup()) return true;
		curValue = _mm256_andnot_si256(bits0, curValue);
		curValue = _mm256_or_si256(curValue, _mm256_or_si256(bits1, _mm256_or_si256(_mm256_srli_epi32(bits3, 1), _mm256_srli_epi32(bits2, 2))));
		if(nextGroup()) return true;
		curValue = _mm256_andnot_si256(bits0, curValue);
		curValue = _mm256_or_si256(curValue, _mm256_or_si256(bits2, _mm256_or_si256(_mm256_srli_epi32(bits1, 1), _mm256_srli_epi32(bits3, 2))));
		if(nextGroup()) return true;
		curValue = _mm256_andnot_si256(bits0, curValue);
		curValue = _mm256_or_si256(curValue, _mm256_or_si256(bits2, _mm256_or_si256(_mm256_srli_epi32(bits3, 1), _mm256_srli_epi32(bits1, 2))));
		if(nextGroup()) return true;
		curValue = _mm256_andnot_si256(bits0, curValue);
		curValue = _mm256_or_si256(curValue, _mm256_or_si256(bits3, _mm256_or_si256(_mm256_srli_epi32(bits2, 1), _mm256_srli_epi32(bits1, 2))));
		if(nextGroup()) return true;
		curValue = _mm256_andnot_si256(bits0, curValue);
		curValue = _mm256_or_si256(curValue, _mm256_or_si256(bits3, _mm256_or_si256(_mm256_srli_epi32(bits1, 1), _mm256_srli_epi32(bits2, 2))));
		if(nextGroup()) return true;
		curValue = _mm256_andnot_si256(bits0, curValue);
		return false;
	}
	inline bool permute4() {
		curValue = _mm256_or_si256(curValue, _mm256_or_si256(bits1, _mm256_or_si256(_mm256_srli_epi32(bits2, 1), _mm256_or_si256(_mm256_srli_epi32(bits3, 2), _mm256_srli_epi32(bits4, 3)))));
		if(nextGroup()) return true;
		curValue = _mm256_andnot_si256(bits0, curValue);
		curValue = _mm256_or_si256(curValue, _mm256_or_si256(bits1, _mm256_or_si256(_mm256_srli_epi32(bits2, 1), _mm256_or_si256(_mm256_srli_epi32(bits4, 2), _mm256_srli_epi32(bits3, 3)))));
		if(nextGroup()) return true;
		curValue = _mm256_andnot_si256(bits0, curValue);
		curValue = _mm256_or_si256(curValue, _mm256_or_si256(bits1, _mm256_or_si256(_mm256_srli_epi32(bits3, 1), _mm256_or_si256(_mm256_srli_epi32(bits2, 2), _mm256_srli_epi32(bits4, 3)))));
		if(nextGroup()) return true;
		curValue = _mm256_andnot_si256(bits0, curValue);
		curValue = _mm256_or_si256(curValue, _mm256_or_si256(bits1, _mm256_or_si256(_mm256_srli_epi32(bits3, 1), _mm256_or_si256(_mm256_srli_epi32(bits4, 2), _mm256_srli_epi32(bits2, 3)))));
		if(nextGroup()) return true;
		curValue = _mm256_andnot_si256(bits0, curValue);
		curValue = _mm256_or_si256(curValue, _mm256_or_si256(bits1, _mm256_or_si256(_mm256_srli_epi32(bits4, 1), _mm256_or_si256(_mm256_srli_epi32(bits3, 2), _mm256_srli_epi32(bits2, 3)))));
		if(nextGroup()) return true;
		curValue = _mm256_andnot_si256(bits0, curValue);
		curValue = _mm256_or_si256(curValue, _mm256_or_si256(bits1, _mm256_or_si256(_mm256_srli_epi32(bits4, 1), _mm256_or_si256(_mm256_srli_epi32(bits2, 2), _mm256_srli_epi32(bits3, 3)))));
		if(nextGroup()) return true;
		curValue = _mm256_andnot_si256(bits0, curValue);
		curValue = _mm256_or_si256(curValue, _mm256_or_si256(bits2, _mm256_or_si256(_mm256_srli_epi32(bits1, 1), _mm256_or_si256(_mm256_srli_epi32(bits3, 2), _mm256_srli_epi32(bits4, 3)))));
		if(nextGroup()) return true;
		curValue = _mm256_andnot_si256(bits0, curValue);
		curValue = _mm256_or_si256(curValue, _mm256_or_si256(bits2, _mm256_or_si256(_mm256_srli_epi32(bits1, 1), _mm256_or_si256(_mm256_srli_epi32(bits4, 2), _mm256_srli_epi32(bits3, 3)))));
		if(nextGroup()) return true;
		curValue = _mm256_andnot_si256(bits0, curValue);
		curValue = _mm256_or_si256(curValue, _mm256_or_si256(bits2, _mm256_or_si256(_mm256_srli_epi32(bits3, 1), _mm256_or_si256(_mm256_srli_epi32(bits1, 2), _mm256_srli_epi32(bits4, 3)))));
		if(nextGroup()) return true;
		curValue = _mm256_andnot_si256(bits0, curValue);
		curValue = _mm256_or_si256(curValue, _mm256_or_si256(bits2, _mm256_or_si256(_mm256_srli_epi32(bits3, 1), _mm256_or_si256(_mm256_srli_epi32(bits4, 2), _mm256_srli_epi32(bits1, 3)))));
		if(nextGroup()) return true;
		curValue = _mm256_andnot_si256(bits0, curValue);
		curValue = _mm256_or_si256(curValue, _mm256_or_si256(bits2, _mm256_or_si256(_mm256_srli_epi32(bits4, 1), _mm256_or_si256(_mm256_srli_epi32(bits3, 2), _mm256_srli_epi32(bits1, 3)))));
		if(nextGroup()) return true;
		curValue = _mm256_andnot_si256(bits0, curValue);
		curValue = _mm256_or_si256(curValue, _mm256_or_si256(bits2, _mm256_or_si256(_mm256_srli_epi32(bits4, 1), _mm256_or_si256(_mm256_srli_epi32(bits1, 2), _mm256_srli_epi32(bits3, 3)))));
		if(nextGroup()) return true;
		curValue = _mm256_andnot_si256(bits0, curValue);
		curValue = _mm256_or_si256(curValue, _mm256_or_si256(bits3, _mm256_or_si256(_mm256_srli_epi32(bits2, 1), _mm256_or_si256(_mm256_srli_epi32(bits1, 2), _mm256_srli_epi32(bits4, 3)))));
		if(nextGroup()) return true;
		curValue = _mm256_andnot_si256(bits0, curValue);
		curValue = _mm256_or_si256(curValue, _mm256_or_si256(bits3, _mm256_or_si256(_mm256_srli_epi32(bits2, 1), _mm256_or_si256(_mm256_srli_epi32(bits4, 2), _mm256_srli_epi32(bits1, 3)))));
		if(nextGroup()) return true;
		curValue = _mm256_andnot_si256(bits0, curValue);
		curValue = _mm256_or_si256(curValue, _mm256_or_si256(bits3, _mm256_or_si256(_mm256_srli_epi32(bits1, 1), _mm256_or_si256(_mm256_srli_epi32(bits2, 2), _mm256_srli_epi32(bits4, 3)))));
		if(nextGroup()) return true;
		curValue = _mm256_andnot_si256(bits0, curValue);
		curValue = _mm256_or_si256(curValue, _mm256_or_si256(bits3, _mm256_or_si256(_mm256_srli_epi32(bits1, 1), _mm256_or_si256(_mm256_srli_epi32(bits4, 2), _mm256_srli_epi32(bits2, 3)))));
		if(nextGroup()) return true;
		curValue = _mm256_andnot_si256(bits0, curValue);
		curValue = _mm256_or_si256(curValue, _mm256_or_si256(bits3, _mm256_or_si256(_mm256_srli_epi32(bits4, 1), _mm256_or_si256(_mm256_srli_epi32(bits1, 2), _mm256_srli_epi32(bits2, 3)))));
		if(nextGroup()) return true;
		curValue = _mm256_andnot_si256(bits0, curValue);
		curValue = _mm256_or_si256(curValue, _mm256_or_si256(bits3, _mm256_or_si256(_mm256_srli_epi32(bits4, 1), _mm256_or_si256(_mm256_srli_epi32(bits2, 2), _mm256_srli_epi32(bits1, 3)))));
		if(nextGroup()) return true;
		curValue = _mm256_andnot_si256(bits0, curValue);
		curValue = _mm256_or_si256(curValue, _mm256_or_si256(bits4, _mm256_or_si256(_mm256_srli_epi32(bits2, 1), _mm256_or_si256(_mm256_srli_epi32(bits3, 2), _mm256_srli_epi32(bits1, 3)))));
		if(nextGroup()) return true;
		curValue = _mm256_andnot_si256(bits0, curValue);
		curValue = _mm256_or_si256(curValue, _mm256_or_si256(bits4, _mm256_or_si256(_mm256_srli_epi32(bits2, 1), _mm256_or_si256(_mm256_srli_epi32(bits1, 2), _mm256_srli_epi32(bits3, 3)))));
		if(nextGroup()) return true;
		curValue = _mm256_andnot_si256(bits0, curValue);
		curValue = _mm256_or_si256(curValue, _mm256_or_si256(bits4, _mm256_or_si256(_mm256_srli_epi32(bits3, 1), _mm256_or_si256(_mm256_srli_epi32(bits2, 2), _mm256_srli_epi32(bits1, 3)))));
		if(nextGroup()) return true;
		curValue = _mm256_andnot_si256(bits0, curValue);
		curValue = _mm256_or_si256(curValue, _mm256_or_si256(bits4, _mm256_or_si256(_mm256_srli_epi32(bits3, 1), _mm256_or_si256(_mm256_srli_epi32(bits1, 2), _mm256_srli_epi32(bits2, 3)))));
		if(nextGroup()) return true;
		curValue = _mm256_andnot_si256(bits0, curValue);
		curValue = _mm256_or_si256(curValue, _mm256_or_si256(bits4, _mm256_or_si256(_mm256_srli_epi32(bits1, 1), _mm256_or_si256(_mm256_srli_epi32(bits3, 2), _mm256_srli_epi32(bits2, 3)))));
		if(nextGroup()) return true;
		curValue = _mm256_andnot_si256(bits0, curValue);
		curValue = _mm256_or_si256(curValue, _mm256_or_si256(bits4, _mm256_or_si256(_mm256_srli_epi32(bits1, 1), _mm256_or_si256(_mm256_srli_epi32(bits2, 2), _mm256_srli_epi32(bits3, 3)))));
		if(nextGroup()) return true;
		curValue = _mm256_andnot_si256(bits0, curValue);
		return false;
	}
	inline bool permute5() {
		curValue = _mm256_or_si256(curValue, _mm256_srli_epi32(bits5, 4));
		if(permute4()) return true;
		curValue = _mm256_andnot_si256(_mm256_srli_epi32(bits5, 4), curValue);
		SWAP(bits1, bits5);
		curValue = _mm256_or_si256(curValue, _mm256_srli_epi32(bits5, 4));
		if(permute4()) return true;
		curValue = _mm256_andnot_si256(_mm256_srli_epi32(bits5, 4), curValue);
		ROTRIGHT(bits1, bits2, bits5);
		curValue = _mm256_or_si256(curValue, _mm256_srli_epi32(bits5, 4));
		if(permute4()) return true;
		curValue = _mm256_andnot_si256(_mm256_srli_epi32(bits5, 4), curValue);
		ROTRIGHT(bits2, bits3, bits5);
		curValue = _mm256_or_si256(curValue, _mm256_srli_epi32(bits5, 4));
		if(permute4()) return true;
		curValue = _mm256_andnot_si256(_mm256_srli_epi32(bits5, 4), curValue);
		ROTRIGHT(bits3, bits4, bits5);
		curValue = _mm256_or_si256(curValue, _mm256_srli_epi32(bits5, 4));
		if(permute4()) return true;
		curValue = _mm256_andnot_si256(_mm256_srli_epi32(bits5, 4), curValue);
		SWAP(bits4, bits5);
		return false;
	}
	inline bool permute6() {
		curValue = _mm256_or_si256(curValue, _mm256_srli_epi32(bits6, 5));
		if(permute5()) return true;
		curValue = _mm256_andnot_si256(_mm256_srli_epi32(bits6, 5), curValue);
		SWAP(bits1, bits6);
		curValue = _mm256_or_si256(curValue, _mm256_srli_epi32(bits6, 5));
		if(permute5()) return true;
		curValue = _mm256_andnot_si256(_mm256_srli_epi32(bits6, 5), curValue);
		ROTRIGHT(bits1, bits2, bits6);
		curValue = _mm256_or_si256(curValue, _mm256_srli_epi32(bits6, 5));
		if(permute5()) return true;
		curValue = _mm256_andnot_si256(_mm256_srli_epi32(bits6, 5), curValue);
		ROTRIGHT(bits2, bits3, bits6);
		curValue = _mm256_or_si256(curValue, _mm256_srli_epi32(bits6, 5));
		if(permute5()) return true;
		curValue = _mm256_andnot_si256(_mm256_srli_epi32(bits6, 5), curValue);
		ROTRIGHT(bits3, bits4, bits6);
		curValue = _mm256_or_si256(curValue, _mm256_srli_epi32(bits6, 5));
		if(permute5()) return true;
		curValue = _mm256_andnot_si256(_mm256_srli_epi32(bits6, 5), curValue);
		ROTRIGHT(bits4, bits5, bits6);
		curValue = _mm256_or_si256(curValue, _mm256_srli_epi32(bits6, 5));
		if(permute5()) return true;
		curValue = _mm256_andnot_si256(_mm256_srli_epi32(bits6, 5), curValue);
		SWAP(bits5, bits6);
		return false;
	}
	inline bool permute7() {
		curValue = _mm256_or_si256(curValue, _mm256_srli_epi32(bits7, 6));
		if(permute6()) return true;
		curValue = _mm256_andnot_si256(_mm256_srli_epi32(bits7, 6), curValue);
		SWAP(bits1, bits7);
		curValue = _mm256_or_si256(curValue, _mm256_srli_epi32(bits7, 6));
		if(permute6()) return true;
		curValue = _mm256_andnot_si256(_mm256_srli_epi32(bits7, 6), curValue);
		ROTRIGHT(bits1, bits2, bits7);
		curValue = _mm256_or_si256(curValue, _mm256_srli_epi32(bits7, 6));
		if(permute6()) return true;
		curValue = _mm256_andnot_si256(_mm256_srli_epi32(bits7, 6), curValue);
		ROTRIGHT(bits2, bits3, bits7);
		curValue = _mm256_or_si256(curValue, _mm256_srli_epi32(bits7, 6));
		if(permute6()) return true;
		curValue = _mm256_andnot_si256(_mm256_srli_epi32(bits7, 6), curValue);
		ROTRIGHT(bits3, bits4, bits7);
		curValue = _mm256_or_si256(curValue, _mm256_srli_epi32(bits7, 6));
		if(permute6()) return true;
		curValue = _mm256_andnot_si256(_mm256_srli_epi32(bits7, 6), curValue);
		ROTRIGHT(bits4, bits5, bits7);
		curValue = _mm256_or_si256(curValue, _mm256_srli_epi32(bits7, 6));
		if(permute6()) return true;
		curValue = _mm256_andnot_si256(_mm256_srli_epi32(bits7, 6), curValue);
		ROTRIGHT(bits5, bits6, bits7);
		curValue = _mm256_or_si256(curValue, _mm256_srli_epi32(bits7, 6));
		if(permute6()) return true;
		curValue = _mm256_andnot_si256(_mm256_srli_epi32(bits7, 6), curValue);
		SWAP(bits6, bits7);
		return false;
	}
	inline bool permute8() {
		curValue = _mm256_or_si256(curValue, _mm256_srli_epi32(bits8, 7));
		if(permute7()) return true;
		curValue = _mm256_andnot_si256(_mm256_srli_epi32(bits8, 7), curValue);
		SWAP(bits1, bits8);
		curValue = _mm256_or_si256(curValue, _mm256_srli_epi32(bits8, 7));
		if(permute7()) return true;
		curValue = _mm256_andnot_si256(_mm256_srli_epi32(bits8, 7), curValue);
		ROTRIGHT(bits1, bits2, bits8);
		curValue = _mm256_or_si256(curValue, _mm256_srli_epi32(bits8, 7));
		if(permute7()) return true;
		curValue = _mm256_andnot_si256(_mm256_srli_epi32(bits8, 7), curValue);
		ROTRIGHT(bits2, bits3, bits8);
		curValue = _mm256_or_si256(curValue, _mm256_srli_epi32(bits8, 7));
		if(permute7()) return true;
		curValue = _mm256_andnot_si256(_mm256_srli_epi32(bits8, 7), curValue);
		ROTRIGHT(bits3, bits4, bits8);
		curValue = _mm256_or_si256(curValue, _mm256_srli_epi32(bits8, 7));
		if(permute7()) return true;
		curValue = _mm256_andnot_si256(_mm256_srli_epi32(bits8, 7), curValue);
		ROTRIGHT(bits4, bits5, bits8);
		curValue = _mm256_or_si256(curValue, _mm256_srli_epi32(bits8, 7));
		if(permute7()) return true;
		curValue = _mm256_andnot_si256(_mm256_srli_epi32(bits8, 7), curValue);
		ROTRIGHT(bits5, bits6, bits8);
		curValue = _mm256_or_si256(curValue, _mm256_srli_epi32(bits8, 7));
		if(permute7()) return true;
		curValue = _mm256_andnot_si256(_mm256_srli_epi32(bits8, 7), curValue);
		ROTRIGHT(bits6, bits7, bits8);
		curValue = _mm256_or_si256(curValue, _mm256_srli_epi32(bits8, 7));
		if(permute7()) return true;
		curValue = _mm256_andnot_si256(_mm256_srli_epi32(bits8, 7), curValue);
		SWAP(bits7, bits8);
		return false;
	}
	inline bool permute9() {
		curValue = _mm256_or_si256(curValue, _mm256_srli_epi32(bits9, 8));
		if(permute8()) return true;
		curValue = _mm256_andnot_si256(_mm256_srli_epi32(bits9, 8), curValue);
		SWAP(bits1, bits9);
		curValue = _mm256_or_si256(curValue, _mm256_srli_epi32(bits9, 8));
		if(permute8()) return true;
		curValue = _mm256_andnot_si256(_mm256_srli_epi32(bits9, 8), curValue);
		ROTRIGHT(bits1, bits2, bits9);
		curValue = _mm256_or_si256(curValue, _mm256_srli_epi32(bits9, 8));
		if(permute8()) return true;
		curValue = _mm256_andnot_si256(_mm256_srli_epi32(bits9, 8), curValue);
		ROTRIGHT(bits2, bits3, bits9);
		curValue = _mm256_or_si256(curValue, _mm256_srli_epi32(bits9, 8));
		if(permute8()) return true;
		curValue = _mm256_andnot_si256(_mm256_srli_epi32(bits9, 8), curValue);
		ROTRIGHT(bits3, bits4, bits9);
		curValue = _mm256_or_si256(curValue, _mm256_srli_epi32(bits9, 8));
		if(permute8()) return true;
		curValue = _mm256_andnot_si256(_mm256_srli_epi32(bits9, 8), curValue);
		ROTRIGHT(bits4, bits5, bits9);
		curValue = _mm256_or_si256(curValue, _mm256_srli_epi32(bits9, 8));
		if(permute8()) return true;
		curValue = _mm256_andnot_si256(_mm256_srli_epi32(bits9, 8), curValue);
		ROTRIGHT(bits5, bits6, bits9);
		curValue = _mm256_or_si256(curValue, _mm256_srli_epi32(bits9, 8));
		if(permute8()) return true;
		curValue = _mm256_andnot_si256(_mm256_srli_epi32(bits9, 8), curValue);
		ROTRIGHT(bits6, bits7, bits9);
		curValue = _mm256_or_si256(curValue, _mm256_srli_epi32(bits9, 8));
		if(permute8()) return true;
		curValue = _mm256_andnot_si256(_mm256_srli_epi32(bits9, 8), curValue);
		ROTRIGHT(bits7, bits8, bits9);
		curValue = _mm256_or_si256(curValue, _mm256_srli_epi32(bits9, 8));
		if(permute8()) return true;
		curValue = _mm256_andnot_si256(_mm256_srli_epi32(bits9, 8), curValue);
		SWAP(bits8, bits9);
		return false;
	}
};

bool SmallEqualityChecker::checkAllForGroup() {
	int curGroupSize = (*groupIter).groupSize;
	++groupIter;
	switch(curGroupSize) {
	case 1:
		bits15 = bits14; bits14 = bits13; bits13 = bits12; bits12 = bits11; bits11 = bits10; bits10 = bits9; bits9 = bits8; bits8 = bits7; bits7 = bits6; bits6 = bits5; bits5 = bits4; bits4 = bits3; bits3 = bits2; bits2 = bits1; bits1 = bits0;
		bits0 = _mm256_and_si256(data, curMask);
		curMask = _mm256_slli_epi32(curMask, 1);
		if(permute1()) return true;
		curMask = _mm256_srli_epi32(curMask, 1);
		bits0 = bits1; bits1 = bits2; bits2 = bits3; bits3 = bits4; bits4 = bits5; bits5 = bits6; bits6 = bits7; bits7 = bits8; bits8 = bits9; bits9 = bits10; bits10 = bits11; bits11 = bits12; bits12 = bits13; bits13 = bits14; bits14 = bits15;
		break;
	case 2:
		bits15 = bits12; bits14 = bits11; bits13 = bits10; bits12 = bits9; bits11 = bits8; bits10 = bits7; bits9 = bits6; bits8 = bits5; bits7 = bits4; bits6 = bits3; bits5 = bits2; bits4 = bits1; bits3 = bits0;
		bits0 = _mm256_sub_epi32(_mm256_slli_epi32(curMask, 2), curMask);
		bits1 = _mm256_slli_epi32(_mm256_and_si256(_mm256_srli_epi32(data, 1), curMask), 1); // TODO optimize, perhaps unneeded shift
		bits2 = _mm256_slli_epi32(_mm256_and_si256(data, curMask), 1); // TODO optimize, perhaps unneeded shift
		curMask = _mm256_slli_epi32(curMask, 2);
		if(permute2()) return true;
		curMask = _mm256_srli_epi32(curMask, 2);
		bits0 = bits3; bits1 = bits4; bits2 = bits5; bits3 = bits6; bits4 = bits7; bits5 = bits8; bits6 = bits9; bits7 = bits10; bits8 = bits11; bits9 = bits12; bits10 = bits13; bits11 = bits14; bits12 = bits15;
		break;
	case 3:
		bits15 = bits11; bits14 = bits10; bits13 = bits9; bits12 = bits8; bits11 = bits7; bits10 = bits6; bits9 = bits5; bits8 = bits4; bits7 = bits3; bits6 = bits2; bits5 = bits1; bits4 = bits0;
		bits0 = _mm256_sub_epi32(_mm256_slli_epi32(curMask, 3), curMask);
		bits1 = _mm256_slli_epi32(_mm256_and_si256(_mm256_srli_epi32(data, 2), curMask), 2); // TODO optimize, perhaps unneeded shift
		bits2 = _mm256_slli_epi32(_mm256_and_si256(_mm256_srli_epi32(data, 1), curMask), 2); // TODO optimize, perhaps unneeded shift
		bits3 = _mm256_slli_epi32(_mm256_and_si256(data, curMask), 2); // TODO optimize, perhaps unneeded shift
		curMask = _mm256_slli_epi32(curMask, 3);
		if(permute3()) return true;
		curMask = _mm256_srli_epi32(curMask, 3);
		bits0 = bits4; bits1 = bits5; bits2 = bits6; bits3 = bits7; bits4 = bits8; bits5 = bits9; bits6 = bits10; bits7 = bits11; bits8 = bits12; bits9 = bits13; bits10 = bits14; bits11 = bits15;
		break;
	case 4:
		bits15 = bits10; bits14 = bits9; bits13 = bits8; bits12 = bits7; bits11 = bits6; bits10 = bits5; bits9 = bits4; bits8 = bits3; bits7 = bits2; bits6 = bits1; bits5 = bits0;
		bits0 = _mm256_sub_epi32(_mm256_slli_epi32(curMask, 4), curMask);
		bits1 = _mm256_slli_epi32(_mm256_and_si256(_mm256_srli_epi32(data, 3), curMask), 3); // TODO optimize, perhaps unneeded shift
		bits2 = _mm256_slli_epi32(_mm256_and_si256(_mm256_srli_epi32(data, 2), curMask), 3); // TODO optimize, perhaps unneeded shift
		bits3 = _mm256_slli_epi32(_mm256_and_si256(_mm256_srli_epi32(data, 1), curMask), 3); // TODO optimize, perhaps unneeded shift
		bits4 = _mm256_slli_epi32(_mm256_and_si256(data, curMask), 3); // TODO optimize, perhaps unneeded shift
		curMask = _mm256_slli_epi32(curMask, 4);
		if(permute4()) return true;
		curMask = _mm256_srli_epi32(curMask, 4);
		bits0 = bits5; bits1 = bits6; bits2 = bits7; bits3 = bits8; bits4 = bits9; bits5 = bits10; bits6 = bits11; bits7 = bits12; bits8 = bits13; bits9 = bits14; bits10 = bits15;
		break;
	case 5:
		bits15 = bits9; bits14 = bits8; bits13 = bits7; bits12 = bits6; bits11 = bits5; bits10 = bits4; bits9 = bits3; bits8 = bits2; bits7 = bits1; bits6 = bits0;
		bits0 = _mm256_slli_epi32(_mm256_sub_epi32(_mm256_slli_epi32(curMask, 4), curMask), 1);
		bits1 = _mm256_slli_epi32(_mm256_and_si256(_mm256_srli_epi32(data, 4), curMask), 4); // TODO optimize, perhaps unneeded shift
		bits2 = _mm256_slli_epi32(_mm256_and_si256(_mm256_srli_epi32(data, 3), curMask), 4); // TODO optimize, perhaps unneeded shift
		bits3 = _mm256_slli_epi32(_mm256_and_si256(_mm256_srli_epi32(data, 2), curMask), 4); // TODO optimize, perhaps unneeded shift
		bits4 = _mm256_slli_epi32(_mm256_and_si256(_mm256_srli_epi32(data, 1), curMask), 4); // TODO optimize, perhaps unneeded shift
		bits5 = _mm256_slli_epi32(_mm256_and_si256(data, curMask), 4); // TODO optimize, perhaps unneeded shift
		curMask = _mm256_slli_epi32(curMask, 5);
		if(permute5()) return true;
		curMask = _mm256_srli_epi32(curMask, 5);
		bits0 = bits6; bits1 = bits7; bits2 = bits8; bits3 = bits9; bits4 = bits10; bits5 = bits11; bits6 = bits12; bits7 = bits13; bits8 = bits14; bits9 = bits15;
		break;
	case 6:
		bits15 = bits8; bits14 = bits7; bits13 = bits6; bits12 = bits5; bits11 = bits4; bits10 = bits3; bits9 = bits2; bits8 = bits1; bits7 = bits0;
		bits0 = _mm256_slli_epi32(_mm256_sub_epi32(_mm256_slli_epi32(curMask, 4), curMask), 2);
		bits1 = _mm256_slli_epi32(_mm256_and_si256(_mm256_srli_epi32(data, 5), curMask), 5); // TODO optimize, perhaps unneeded shift
		bits2 = _mm256_slli_epi32(_mm256_and_si256(_mm256_srli_epi32(data, 4), curMask), 5); // TODO optimize, perhaps unneeded shift
		bits3 = _mm256_slli_epi32(_mm256_and_si256(_mm256_srli_epi32(data, 3), curMask), 5); // TODO optimize, perhaps unneeded shift
		bits4 = _mm256_slli_epi32(_mm256_and_si256(_mm256_srli_epi32(data, 2), curMask), 5); // TODO optimize, perhaps unneeded shift
		bits5 = _mm256_slli_epi32(_mm256_and_si256(_mm256_srli_epi32(data, 1), curMask), 5); // TODO optimize, perhaps unneeded shift
		bits6 = _mm256_slli_epi32(_mm256_and_si256(data, curMask), 5); // TODO optimize, perhaps unneeded shift
		curMask = _mm256_slli_epi32(curMask, 6);
		if(permute6()) return true;
		curMask = _mm256_srli_epi32(curMask, 6);
		bits0 = bits7; bits1 = bits8; bits2 = bits9; bits3 = bits10; bits4 = bits11; bits5 = bits12; bits6 = bits13; bits7 = bits14; bits8 = bits15;
		break;
	case 7:
		bits15 = bits7; bits14 = bits6; bits13 = bits5; bits12 = bits4; bits11 = bits3; bits10 = bits2; bits9 = bits1; bits8 = bits0;
		bits0 = _mm256_slli_epi32(_mm256_sub_epi32(_mm256_slli_epi32(curMask, 4), curMask), 3);
		bits1 = _mm256_slli_epi32(_mm256_and_si256(_mm256_srli_epi32(data, 6), curMask), 6); // TODO optimize, perhaps unneeded shift
		bits2 = _mm256_slli_epi32(_mm256_and_si256(_mm256_srli_epi32(data, 5), curMask), 6); // TODO optimize, perhaps unneeded shift
		bits3 = _mm256_slli_epi32(_mm256_and_si256(_mm256_srli_epi32(data, 4), curMask), 6); // TODO optimize, perhaps unneeded shift
		bits4 = _mm256_slli_epi32(_mm256_and_si256(_mm256_srli_epi32(data, 3), curMask), 6); // TODO optimize, perhaps unneeded shift
		bits5 = _mm256_slli_epi32(_mm256_and_si256(_mm256_srli_epi32(data, 2), curMask), 6); // TODO optimize, perhaps unneeded shift
		bits6 = _mm256_slli_epi32(_mm256_and_si256(_mm256_srli_epi32(data, 1), curMask), 6); // TODO optimize, perhaps unneeded shift
		bits7 = _mm256_slli_epi32(_mm256_and_si256(data, curMask), 6); // TODO optimize, perhaps unneeded shift
		curMask = _mm256_slli_epi32(curMask, 7);
		if(permute7()) return true;
		curMask = _mm256_srli_epi32(curMask, 7);
		bits0 = bits8; bits1 = bits9; bits2 = bits10; bits3 = bits11; bits4 = bits12; bits5 = bits13; bits6 = bits14; bits7 = bits15;
		break;
	case 8:
		bits15 = bits6; bits14 = bits5; bits13 = bits4; bits12 = bits3; bits11 = bits2; bits10 = bits1; bits9 = bits0;
		bits0 = _mm256_slli_epi32(_mm256_sub_epi32(_mm256_slli_epi32(curMask, 4), curMask), 4);
		bits1 = _mm256_slli_epi32(_mm256_and_si256(_mm256_srli_epi32(data, 7), curMask), 7); // TODO optimize, perhaps unneeded shift
		bits2 = _mm256_slli_epi32(_mm256_and_si256(_mm256_srli_epi32(data, 6), curMask), 7); // TODO optimize, perhaps unneeded shift
		bits3 = _mm256_slli_epi32(_mm256_and_si256(_mm256_srli_epi32(data, 5), curMask), 7); // TODO optimize, perhaps unneeded shift
		bits4 = _mm256_slli_epi32(_mm256_and_si256(_mm256_srli_epi32(data, 4), curMask), 7); // TODO optimize, perhaps unneeded shift
		bits5 = _mm256_slli_epi32(_mm256_and_si256(_mm256_srli_epi32(data, 3), curMask), 7); // TODO optimize, perhaps unneeded shift
		bits6 = _mm256_slli_epi32(_mm256_and_si256(_mm256_srli_epi32(data, 2), curMask), 7); // TODO optimize, perhaps unneeded shift
		bits7 = _mm256_slli_epi32(_mm256_and_si256(_mm256_srli_epi32(data, 1), curMask), 7); // TODO optimize, perhaps unneeded shift
		bits8 = _mm256_slli_epi32(_mm256_and_si256(data, curMask), 7); // TODO optimize, perhaps unneeded shift
		curMask = _mm256_slli_epi32(curMask, 8);
		if(permute8()) return true;
		curMask = _mm256_srli_epi32(curMask, 8);
		bits0 = bits9; bits1 = bits10; bits2 = bits11; bits3 = bits12; bits4 = bits13; bits5 = bits14; bits6 = bits15;
		break;
	case 9:
		bits15 = bits5; bits14 = bits4; bits13 = bits3; bits12 = bits2; bits11 = bits1; bits10 = bits0;
		bits0 = _mm256_slli_epi32(_mm256_sub_epi32(_mm256_slli_epi32(curMask, 4), curMask), 5);
		bits1 = _mm256_slli_epi32(_mm256_and_si256(_mm256_srli_epi32(data, 8), curMask), 8); // TODO optimize, perhaps unneeded shift
		bits2 = _mm256_slli_epi32(_mm256_and_si256(_mm256_srli_epi32(data, 7), curMask), 8); // TODO optimize, perhaps unneeded shift
		bits3 = _mm256_slli_epi32(_mm256_and_si256(_mm256_srli_epi32(data, 6), curMask), 8); // TODO optimize, perhaps unneeded shift
		bits4 = _mm256_slli_epi32(_mm256_and_si256(_mm256_srli_epi32(data, 5), curMask), 8); // TODO optimize, perhaps unneeded shift
		bits5 = _mm256_slli_epi32(_mm256_and_si256(_mm256_srli_epi32(data, 4), curMask), 8); // TODO optimize, perhaps unneeded shift
		bits6 = _mm256_slli_epi32(_mm256_and_si256(_mm256_srli_epi32(data, 3), curMask), 8); // TODO optimize, perhaps unneeded shift
		bits7 = _mm256_slli_epi32(_mm256_and_si256(_mm256_srli_epi32(data, 2), curMask), 8); // TODO optimize, perhaps unneeded shift
		bits8 = _mm256_slli_epi32(_mm256_and_si256(_mm256_srli_epi32(data, 1), curMask), 8); // TODO optimize, perhaps unneeded shift
		bits9 = _mm256_slli_epi32(_mm256_and_si256(data, curMask), 8); // TODO optimize, perhaps unneeded shift
		curMask = _mm256_slli_epi32(curMask, 9);
		if(permute9()) return true;
		curMask = _mm256_srli_epi32(curMask, 9);
		bits0 = bits10; bits1 = bits11; bits2 = bits12; bits3 = bits13; bits4 = bits14; bits5 = bits15;
		break;
	}
	--groupIter;
	return false;
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
bool existsPermutationOverVariableGroupsSIMD(int numberOfVariables, const std::vector<VariableGroup>& groups, __m256i data, const Func& func) {
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

	/*SmallEqualityChecker eqChecker(this->equalityChecker, b.functionInputSet, b.variableOccurences);

	bool newAlgoResult = eqChecker.checkAllForGroup();

	if(!newAlgoResult) return false;

	if(b.functionInputSet.size() <= 8) {*/
		
		//std::cout << (newAlgoResult ? '.' : 'f');
		//return newAlgoResult;

		/*__m256i initialData = _mm256_load_si256(reinterpret_cast<const __m256i*>(b.functionInputSet.getData()));
		bool oldAlgoResult = existsPermutationOverVariableGroupsSIMD(this->spanSize, this->variableOccurences, initialData, [&eqCheck = this->equalityChecker, bsize = b.functionInputSet.size()](__m256i swizzledFirstBlock, int* swizStart, int* swizEnd){
			for(int i = 0; i < bsize; i++) {
				FunctionInput fn{swizzledFirstBlock.m256i_i32[i]};
				if(!eqCheck.contains(fn)) {
					return false;
				}
			}
			return true;
		});*/
		
		//return newAlgoResult;

		/*if(newAlgoResult != oldAlgoResult) {
			std::cout << (newAlgoResult ? 'T' : 'F');
			SmallEqualityChecker eqChecker(this->equalityChecker, b.functionInputSet, b.variableOccurences);

			bool newAlgoResult = eqChecker.checkAllForGroup();
		} else {
			std::cout << (newAlgoResult ? 't' : 'f');
		}

		return oldAlgoResult;*/
	/*} else {
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
	}*/

	std::vector<int> permut = generateIntegers(spanSize);
	return existsPermutationOverVariableGroups(this->variableOccurences, std::move(permut), [this, &bp = b.functionInputSet](const std::vector<int>& permutation) {
		for(FunctionInput fn : bp) {
			if(!equalityChecker.contains(fn.swizzle(permutation))) return false;
		}
		return true;
	});
}
