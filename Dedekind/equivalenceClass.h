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

struct VariableOccurenceGroup {
	int groupSize;
	int occurences;
};
inline bool operator==(VariableOccurenceGroup a, VariableOccurenceGroup b) { return a.groupSize == b.groupSize && a.occurences == b.occurences; }
inline bool operator!=(VariableOccurenceGroup a, VariableOccurenceGroup b) { return a.groupSize != b.groupSize || a.occurences != b.occurences; }

struct PreprocessedFunctionInputSet {
	FunctionInputSet functionInputSet;
	std::vector<VariableOccurenceGroup> variableOccurences;
	int spanSize;

	inline size_t size() const { return functionInputSet.size(); }

	static PreprocessedFunctionInputSet emptyPreprocessedFunctionInputSet;
};

PreprocessedFunctionInputSet preprocess(const FunctionInputSet& inputSet, FunctionInput sp);
inline PreprocessedFunctionInputSet preprocess(const FunctionInputSet& inputSet) {
	FunctionInput sp = span(inputSet);

	return preprocess(inputSet, sp);
}

struct EquivalenceClass : public PreprocessedFunctionInputSet {
	int_set<FunctionInput, FunctionInput::underlyingType> equalityChecker;

	EquivalenceClass() = default;
	
	inline EquivalenceClass(const PreprocessedFunctionInputSet& parent) : PreprocessedFunctionInputSet(parent), equalityChecker(1 << parent.spanSize, parent.functionInputSet) {}
	inline EquivalenceClass(PreprocessedFunctionInputSet&& parent) : PreprocessedFunctionInputSet(parent), equalityChecker(1 << parent.spanSize, parent.functionInputSet) {}

	static EquivalenceClass emptyEquivalenceClass;

	bool contains(const PreprocessedFunctionInputSet& b) const;
};

inline bool isIsomorphic(const PreprocessedFunctionInputSet& a, const PreprocessedFunctionInputSet& b) {
	assert(a.size() == b.size()); // assume we are only comparing functionInputSets with the same number of edges

	if(a.spanSize != b.spanSize) return false; // early exit, a and b do not span the same number of variables, impossible to be isomorphic!

	return EquivalenceClass(a).contains(b);
}

inline bool isIsomorphic(const PreprocessedFunctionInputSet& a, const FunctionInputSet& b) {
	assert(a.size() == b.size()); // assume we are only comparing functionInputSets with the same number of edges

	FunctionInput spb = span(b);

	if(a.spanSize != spb.getNumberEnabled()) return false; // early exit, a and b do not span the same number of variables, impossible to be isomorphic!

	PreprocessedFunctionInputSet bn = preprocess(b, spb);

	return isIsomorphic(a, bn);
}

inline bool isIsomorphic(const FunctionInputSet& a, const FunctionInputSet& b) {
	assert(a.size() == b.size()); // assume we are only comparing functionInputSets with the same number of edges

	FunctionInput spa = span(a);
	FunctionInput spb = span(b);

	if(spa.getNumberEnabled() != spb.getNumberEnabled()) return false; // early exit, a and b do not span the same number of variables, impossible to be isomorphic!

	PreprocessedFunctionInputSet an = preprocess(a, spa);

	return isIsomorphic(an, b);
}

