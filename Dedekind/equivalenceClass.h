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

struct PreprocessedFunctionInputSet {
	InputBitSet functionInputSet;

	inline size_t size() const { return functionInputSet.size(); }

	PreprocessedFunctionInputSet extendedBy(FunctionInput inp) const;

	static PreprocessedFunctionInputSet emptyPreprocessedFunctionInputSet;
	static PreprocessedFunctionInputSet bottomPreprocessedFunctionInputSet; // only has one FunctionInput: {/}

	uint64_t hash() const;
};

PreprocessedFunctionInputSet preprocess(FunctionInputSet inputSet);

struct EquivalenceClass {
	InputBitSet functionInputSet;
	EquivalenceClass() = default;
	
	inline EquivalenceClass(const PreprocessedFunctionInputSet& prep) : functionInputSet(prep.functionInputSet) {}

	inline size_t size() const { return functionInputSet.size(); }

	bool contains(const PreprocessedFunctionInputSet& b) const;

	PreprocessedFunctionInputSet extendedBy(FunctionInput fi) const;

	inline bool hasFunctionInput(FunctionInput fi) const {
		return functionInputSet.contains(fi.inputBits);
	}

	FunctionInputSet asFunctionInputSet() const;

	inline uint64_t hash() const { return functionInputSet.hash(); }

	static EquivalenceClass emptyEquivalenceClass;
	static EquivalenceClass bottomEquivalenceClass;
};
