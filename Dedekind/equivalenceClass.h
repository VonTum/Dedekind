#pragma once

#include <cassert>
#include <algorithm>
#include <bitset>

#include "collectionOperations.h"
#include "functionInput.h"
#include "functionInputSet.h"
#include "functionInputBitSet.h"
#include "intSet.h"
#include "smallVector.h"

constexpr int MAX_PREPROCESSED = 7;
constexpr int MAX_LAYER_SIZE = 35;

typedef FunctionInputBitSet<MAX_PREPROCESSED> InputBitSet;

template<typename T>
using PreprocessSmallVector = SmallVector<T, MAX_PREPROCESSED>;
template<typename T>
using LargePreprocessSmallVector = SmallVector<T, MAX_LAYER_SIZE>;


struct VariableCoOccurence {
	LargePreprocessSmallVector<int> coOccursWith;
};
inline bool operator==(const VariableCoOccurence& a, const VariableCoOccurence& b) {
	return a.coOccursWith == b.coOccursWith;
}
inline bool operator!=(const VariableCoOccurence& a, const VariableCoOccurence& b) {
	return !(a == b);
}

struct InitialVariableObservations {
	int occurenceCount;
	int subGraphSize;
};
inline bool operator==(InitialVariableObservations a, InitialVariableObservations b) {
	return a.occurenceCount == b.occurenceCount && a.subGraphSize == b.subGraphSize;
}
inline bool operator!=(InitialVariableObservations a, InitialVariableObservations b) {
	return a.occurenceCount != b.occurenceCount || a.subGraphSize != b.subGraphSize;
}
inline bool operator<(InitialVariableObservations a, InitialVariableObservations b) {
	if(a.subGraphSize == b.subGraphSize) {
		return a.occurenceCount < b.occurenceCount;
	} else {
		return a.subGraphSize < b.subGraphSize;
	}
}
inline bool operator<=(InitialVariableObservations a, InitialVariableObservations b) {
	if(a.subGraphSize == b.subGraphSize) {
		return a.occurenceCount <= b.occurenceCount;
	} else {
		return a.subGraphSize <= b.subGraphSize;
	}
}
inline bool operator>(InitialVariableObservations a, InitialVariableObservations b) { return b < a; }
inline bool operator>=(InitialVariableObservations a, InitialVariableObservations b) { return b <= a; }

struct PreprocessedFunctionInputSet {
	InputBitSet functionInputSet;
	/*PreprocessSmallVector<InitialVariableObservations> variables;
	PreprocessSmallVector<CountedGroup<VariableCoOccurence>> variableCoOccurences;
	int8_t spanSize;
	int8_t variableOccurences[MAX_PREPROCESSED];*/

	inline size_t size() const { return functionInputSet.size(); }

	PreprocessedFunctionInputSet extendedBy(FunctionInput inp) const;

	static PreprocessedFunctionInputSet emptyPreprocessedFunctionInputSet;
	static PreprocessedFunctionInputSet bottomPreprocessedFunctionInputSet; // only has one FunctionInput: {/}

	uint64_t hash() const;
};

PreprocessedFunctionInputSet preprocess(FunctionInputSet inputSet);

struct EquivalenceClass {
	InputBitSet functionInputSet;
	uint64_t hash;
	/*int8_t spanSize;
	int8_t variableOccurences[MAX_PREPROCESSED];
	InitialVariableObservations variables[MAX_PREPROCESSED];*/

	EquivalenceClass() = default;
	
	/*inline EquivalenceClass(const PreprocessedFunctionInputSet& prep, uint64_t hash) :
		functionInputSet(),
		spanSize(prep.spanSize),
		variableOccurences(),
		variables(),
		hash(hash) {
		
		for(int i = 0; i < spanSize; i++) {
			variables[i] = prep.variables[i];
			variableOccurences[i] = prep.variableOccurences[i];
		}
		for(int i = spanSize; i < MAX_PREPROCESSED; i++) {
			variableOccurences[i] = -1;
			variables[i] = InitialVariableObservations{-1,-1};
		}
		for(const FunctionInput& fi : prep.functionInputSet) {
			functionInputSet.set(fi.inputBits);
		}
	}*/
	inline EquivalenceClass(const PreprocessedFunctionInputSet& prep, uint64_t hash) :
		functionInputSet(prep.functionInputSet),
		hash(hash) {}
	
	inline EquivalenceClass(const PreprocessedFunctionInputSet& prep) : EquivalenceClass(prep, prep.hash()) {}


	inline size_t size() const { return functionInputSet.size(); }

	bool contains(const PreprocessedFunctionInputSet& b) const;

	PreprocessedFunctionInputSet extendedBy(FunctionInput fi) const;

	inline bool hasFunctionInput(FunctionInput fi) const {
		return functionInputSet.contains(fi.inputBits);
	}

	FunctionInputSet asFunctionInputSet() const;

	static EquivalenceClass emptyEquivalenceClass;
	static EquivalenceClass bottomEquivalenceClass;
};
