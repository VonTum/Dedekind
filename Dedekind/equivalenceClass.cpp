#include "equivalenceClass.h"

#include <immintrin.h>

#include <iostream>

PreprocessedFunctionInputSet PreprocessedFunctionInputSet::emptyPreprocessedFunctionInputSet = PreprocessedFunctionInputSet{FunctionInputSet{}, PreprocessSmallVector<InitialVariableObservations>{}, PreprocessSmallVector<CountedGroup<VariableCoOccurence>>{}, 0};
EquivalenceClass EquivalenceClass::emptyEquivalenceClass = EquivalenceClass(PreprocessedFunctionInputSet::emptyPreprocessedFunctionInputSet);


// TODO candidate for optimization
PreprocessedFunctionInputSet PreprocessedFunctionInputSet::extendedBy(FunctionInput inp) const {
	FunctionInputSet resultingFuncInputSet(this->functionInputSet.size() + 1);
	for(size_t i = 0; i < this->functionInputSet.size(); i++) resultingFuncInputSet[i] = this->functionInputSet[i];
	resultingFuncInputSet[this->functionInputSet.size()] = inp;
	return preprocess(std::move(resultingFuncInputSet));
}

PreprocessedFunctionInputSet EquivalenceClass::extendedBy(FunctionInput fi) const {
	size_t size = this->functionInputSet.count();
	FunctionInputSet resultingFuncInputSet;
	resultingFuncInputSet.reserve(size + 1);
	for(FunctionInput::underlyingType i = 0; i < (1 << spanSize); i++) {
		if(this->functionInputSet.test(i)) {
			resultingFuncInputSet.push_back(FunctionInput{i});
		}
	}
	resultingFuncInputSet.push_back(fi);
	return preprocess(std::move(resultingFuncInputSet));
}

uint64_t PreprocessedFunctionInputSet::hash() const {
	uint64_t hsh = spanSize;
	for(const CountedGroup<VariableCoOccurence>& cg : variableCoOccurences) {
		uint64_t hshOfCoOccur = 1; // big prime
		int i = 1;
		for(long long v : cg.group.coOccursWith) {
			hshOfCoOccur = hshOfCoOccur * 113 + v*i;
			i++;
		}
		hsh = hsh * 101 + cg.count;
		hsh = hsh * 101 + hshOfCoOccur;
	}
	return hsh ^ (hsh >> 8) ^ (hsh >> 16) ^ (hsh >> 32);
}

FunctionInputSet EquivalenceClass::asFunctionInputSet() const {
	size_t size = functionInputSet.count();
	FunctionInputSet result;
	result.reserve(size);
	for(FunctionInput::underlyingType i = 0; i < size; i++) {
		if(functionInputSet.test(i)) result.push_back(FunctionInput{i});
	}
	return result;
}

static PreprocessSmallVector<int> computeOccurenceCounts(const FunctionInputSet& inputSet, int spanSize) {
	PreprocessSmallVector<int> result(spanSize);

	for(int bit = 0; bit < spanSize; bit++) {
		int total = 0;
		for(FunctionInput f : inputSet) {
			if(f[bit]) {
				total++;
			}
		}
		result[bit] = total;
	}

	return result;
}

// used for generating VariableCoOccurence lists
// skip 2 as it leads to hash collissions
static const long long primes[]{3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47};

__declspec(noinline) static void computeCoOccurences(const PreprocessSmallVector<int>& groupAssignments, const FunctionInputSet& functionInputSet, PreprocessSmallVector<VariableCoOccurence>& result) {
	int variableCount = groupAssignments.size();

	for(int curVar = 0; curVar < variableCount; curVar++) {
		VariableCoOccurence& curVarCoOccurence = result[curVar];
		curVarCoOccurence.coOccursWith.clear();
		size_t curIndexInCoOccursWith = 0;
		for(FunctionInput fi : functionInputSet) {
			if(fi[curVar]) {
				long long total = 1;
				for(int foundVar : fi) {
					if(foundVar == curVar) continue;
					total *= primes[groupAssignments[foundVar]];
				}
				curVarCoOccurence.coOccursWith.push_back(total);
			}
		}

		std::sort(curVarCoOccurence.coOccursWith.begin(), curVarCoOccurence.coOccursWith.end());
	}
}

template<size_t MaxSize>
static int getHighest(const SmallVector<int, MaxSize>& items) {
	int highest = items[0];
	for(int i = 1; i < items.size(); i++) {
		if(items[i] > highest) highest = items[i];
	}
	return highest;
}

bool compareVariableOccurence(const VariableCoOccurence& first, const VariableCoOccurence& second) {
	if(first.coOccursWith.size() != second.coOccursWith.size()) {
		return first.coOccursWith.size() < second.coOccursWith.size();
	} else {
		for(size_t i = 0; i < first.coOccursWith.size(); i++) {
			long long occurFirst = first.coOccursWith[i];
			long long occurSecond = second.coOccursWith[i];
			if(occurFirst != occurSecond) {
				return occurFirst < occurSecond;
			}
		}
		// both are exactly the same
		//default to false
		return false;
	}
}

static std::pair<PreprocessSmallVector<int>, int> findDisjointSubGraphs(FunctionInputSet& functionInputSet, int numberOfVariables) {
	PreprocessSmallVector<int> result(numberOfVariables, -1);

	int curGroup = 0;
	
	for(int startOfGroupVar = 0; startOfGroupVar < numberOfVariables; startOfGroupVar++) {
		if(result[startOfGroupVar] == -1) {

			result[startOfGroupVar] = curGroup;
			FunctionInput curGroupMask{1 << startOfGroupVar};

			bool wasChanged = true;
			while(wasChanged) {
				wasChanged = false;
				for(int var = 0; var < numberOfVariables; var++) {
					if(result[var] != -1) continue; // skip already assigned variables

					for(FunctionInput fi : functionInputSet) {
						if(fi[var] && overlaps(fi, curGroupMask)) {
							result[var] = curGroup;
							curGroupMask.enableInput(var);
							wasChanged = true;
						}
					}
				}
			}
			// group exhausted
			curGroup++;
		}
	}
	return std::make_pair(std::move(result), curGroup);
}

static PreprocessSmallVector<int> countSizeOfSubgraphForEachVariable(FunctionInputSet& functionInputSet, int numberOfVariables) {
	std::pair<PreprocessSmallVector<int>, int> disjointSubGraphs = findDisjointSubGraphs(functionInputSet, numberOfVariables);

	PreprocessSmallVector<int> sizesOfEachGroup(disjointSubGraphs.second, 0); // number of groups

	for(int groupID : disjointSubGraphs.first) {
		sizesOfEachGroup[groupID]++;
	}

	PreprocessSmallVector<int> result = std::move(disjointSubGraphs.first);

	for(int i = 0; i < result.size(); i++) {
		result[i] = sizesOfEachGroup[result[i]];
	}

	return result;
}


struct RefinedVariableSymmetryGroups {
	PreprocessSmallVector<int> groups;
	std::pair<PreprocessSmallVector<int>, PreprocessSmallVector<InitialVariableObservations>> initialVariableInfo;
	PreprocessSmallVector<VariableCoOccurence> coOccurences;
};

static RefinedVariableSymmetryGroups refineVariableSymmetryGroups(const PreprocessSmallVector<InitialVariableObservations>& variables, FunctionInputSet& functionInputSet) {
	PreprocessSmallVector<VariableCoOccurence> coOccurences(variables.size());
	
	std::pair<PreprocessSmallVector<int>, PreprocessSmallVector<InitialVariableObservations>> initialGroups = assignUniqueGroups<PreprocessSmallVector, InitialVariableObservations>(variables);
	PreprocessSmallVector<int> groupIndices = initialGroups.first;
	int oldNumGroups = initialGroups.second.size();

	while(true) {
		computeCoOccurences(groupIndices, functionInputSet, coOccurences);
		PreprocessSmallVector<int> sortPermut(coOccurences.size());
		for(size_t i = 0; i < sortPermut.size(); i++) sortPermut[i] = i;
		getSortPermutation(coOccurences, compareVariableOccurence, sortPermut);
		coOccurences = permute(coOccurences, sortPermut);
		swizzleVector(sortPermut, functionInputSet, functionInputSet);
		std::pair<PreprocessSmallVector<int>, PreprocessSmallVector<VariableCoOccurence>> newGroups = assignUniqueGroups<PreprocessSmallVector, VariableCoOccurence>(coOccurences);
		groupIndices = newGroups.first;
		if(oldNumGroups == newGroups.second.size()) {// stagnation, all groups found. Number of groups can only go up as groups are further split up
			return RefinedVariableSymmetryGroups{std::move(groupIndices), std::move(initialGroups), std::move(coOccurences)};
		}
		oldNumGroups = newGroups.second.size();
	}
}

// returns the resulting functionInputSet, and the number of found variables
static int preprocessRemoveUnusedVariables(FunctionInputSet& inputSet) {
	int inputIndex = 0;
	FunctionInput shiftingSP = span(inputSet);
	PreprocessSmallVector<int> variableRemap;
	while(!shiftingSP.empty()) {
		if(shiftingSP[0]) {
			variableRemap.push_back(inputIndex);
		}
		shiftingSP >>= 1;
		inputIndex++;
	}
	swizzleVector(variableRemap, inputSet, inputSet);
	int numberOfVariables = variableRemap.size();
	assert(span(inputSet) == FunctionInput::allOnes(numberOfVariables));
	return numberOfVariables;
}

template<typename T>
static PreprocessSmallVector<T> sortSwizzle(const PreprocessSmallVector<T>& collectedData, FunctionInputSet& inputSet) {
	PreprocessSmallVector<int> permutation(collectedData.size());
	for(int i = 0; i < collectedData.size(); i++) {
		permutation[i] = i;
	}

	std::sort(permutation.begin(), permutation.end(), [&collectedData](int a, int b) {
		return collectedData[a] < collectedData[b];
	});

	swizzleVector(permutation, inputSet, inputSet);
	return permute(collectedData, permutation);
}

PreprocessedFunctionInputSet preprocess(FunctionInputSet inputSet) {
	if(inputSet.size() == 0 || inputSet[0].empty()) {
		return PreprocessedFunctionInputSet::emptyPreprocessedFunctionInputSet;
	}
	int variableCount = preprocessRemoveUnusedVariables(inputSet);

	PreprocessSmallVector<int> varOccurences = computeOccurenceCounts(inputSet, variableCount);
	PreprocessSmallVector<int> subGraphSizes = countSizeOfSubgraphForEachVariable(inputSet, variableCount);

	PreprocessSmallVector<InitialVariableObservations> variables(variableCount);
	for(int v = 0; v < varOccurences.size(); v++) {variables[v].occurenceCount = varOccurences[v];variables[v].subGraphSize = subGraphSizes[v];}

	variables = sortSwizzle(variables, inputSet);

	RefinedVariableSymmetryGroups resultingRefinedGroups = refineVariableSymmetryGroups(variables, inputSet);

	PreprocessedFunctionInputSet result{
		inputSet,
		variables,
		tallyDistinctOrdered<PreprocessSmallVector>(resultingRefinedGroups.coOccurences.begin(), resultingRefinedGroups.coOccurences.end()), 
		variableCount
	};

	for(size_t i = 0; i < resultingRefinedGroups.groups.size(); i++){
		result.variableOccurences[i] = resultingRefinedGroups.groups[i];
	}

	return result;
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
	assert(this->size() == b.size()); // assume we are only comparing functionInputSets with the same number of edges

	if(this->spanSize != b.spanSize) return false;

	for(int i = 0; i < spanSize; i++) {
		if(this->variables[i] != b.variables[i]) {
			return false;
		}
	}

	for(int i = 0; i < spanSize; i++) {
		if(this->variableOccurences[i] != b.variableOccurences[i]) {
			return false;
		}
	}

	//if(this->variableCoOccurences != b.variableCoOccurences) return false; // early exit, a and b do not span the same variables, impossible to be isomorphic!
	

	//if(this->functionInputSet == b.functionInputSet) return true;




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
	bool realResult = existsPermutationOverVariableGroups(this->variableOccurences, this->variableOccurences + this->spanSize, std::move(permut), [this, &bp = b.functionInputSet](const std::vector<int>& permutation) {
		for(FunctionInput fn : bp) {
			if(!this->functionInputSet.test(fn.swizzle(permutation).inputBits)) return false;
		}
		return true;
	});

	/*if(realResult == false) {
		__debugbreak();
		//throw "normalization is imperfect, multiple FunctionInputSets are mapped to the same Normalized HyperGraph";
	}*/
	return realResult;
}
