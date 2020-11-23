#pragma once

#include <cassert>
#include <algorithm>

#include "collectionOperations.h"
#include "functionInput.h"
#include "functionInputSet.h"
#include "intSet.h"

struct VariableOccurence {
	int index;
	int count;
};

struct VariableCoOccurence {
	std::vector<int> coOccursWith;
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
	FunctionInputSet functionInputSet;
	std::vector<InitialVariableObservations> variables;
	std::vector<int> variableOccurences;
	std::vector<CountedGroup<VariableCoOccurence>> variableCoOccurences;
	int spanSize;

	inline size_t size() const { return functionInputSet.size(); }

	PreprocessedFunctionInputSet extendedBy(FunctionInput inp) const;

	static PreprocessedFunctionInputSet emptyPreprocessedFunctionInputSet;

	uint64_t hash() const;
};

PreprocessedFunctionInputSet preprocess(FunctionInputSet inputSet);

struct EquivalenceClass : public PreprocessedFunctionInputSet {
	int_set<FunctionInput, FunctionInput::underlyingType> equalityChecker;

	EquivalenceClass() = default;
	
	inline EquivalenceClass(const PreprocessedFunctionInputSet& parent) : PreprocessedFunctionInputSet(parent), equalityChecker(1 << parent.spanSize, parent.functionInputSet) {}
	inline EquivalenceClass(PreprocessedFunctionInputSet&& parent) : PreprocessedFunctionInputSet(parent), equalityChecker(1 << parent.spanSize, parent.functionInputSet) {}

	static EquivalenceClass emptyEquivalenceClass;

	bool contains(const PreprocessedFunctionInputSet& b) const;

	inline bool hasFunctionInput(FunctionInput fi) const {
		return equalityChecker.containsFree(fi);
	}
};
