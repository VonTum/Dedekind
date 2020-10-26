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
	std::vector<long long> coOccursWith;
};
inline bool operator==(const VariableCoOccurence& a, const VariableCoOccurence& b) {
	return a.coOccursWith == b.coOccursWith;
}
inline bool operator!=(const VariableCoOccurence& a, const VariableCoOccurence& b) {
	return !(a == b);
}

struct VariableGroup {
	int groupSize;
	int occurences;
};
inline bool operator==(VariableGroup a, VariableGroup b) { return a.groupSize == b.groupSize && a.occurences == b.occurences; }
inline bool operator!=(VariableGroup a, VariableGroup b) { return a.groupSize != b.groupSize || a.occurences != b.occurences; }

struct PreprocessedFunctionInputSet {
	FunctionInputSet functionInputSet;
	std::vector<VariableGroup> variableOccurences;
	std::vector<CountedGroup<VariableCoOccurence>> variableCoOccurences;
	int spanSize;

	inline size_t size() const { return functionInputSet.size(); }

	PreprocessedFunctionInputSet extendedBy(FunctionInput inp) const;

	static PreprocessedFunctionInputSet emptyPreprocessedFunctionInputSet;

	uint64_t hash() const;
};

PreprocessedFunctionInputSet preprocess(const FunctionInputSet& inputSet);

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
