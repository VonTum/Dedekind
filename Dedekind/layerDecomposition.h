#pragma once

#include "equivalenceClass.h"
#include "equivalenceClassMap.h"

#include "heapArray.h"

#include <vector>


typedef uint64_t bigInt;
typedef bigInt countInt;
typedef bigInt valueInt;

struct EquivalenceClassIndex {
	int subLayer : 8;
	int indexInSubLayer : 24;
	EquivalenceClassIndex() : subLayer(-1), indexInSubLayer(-1) {}
	EquivalenceClassIndex(int subLayer, int indexInSubLayer) : subLayer(subLayer), indexInSubLayer(indexInSubLayer) {
		assert(indexInSubLayer >= 0 && indexInSubLayer < (1 << 24));
		assert(subLayer >= 0 && subLayer < 256);
	}
};

struct EquivalenceClassInfo {
	struct NextClass {
		int nodeIndex : 24;
		int formationCount : 8;
		NextClass() : nodeIndex(-1), formationCount(-1) {}
		NextClass(int nodeIndex, int formationCount) : nodeIndex(nodeIndex), formationCount(formationCount) {
			assert(nodeIndex >= 0 && nodeIndex < (1 << 24));
			assert(formationCount >= 0 && formationCount < 256);
		}
	};

	NextClass* nextClasses;
	int numberOfNextClasses;
	int inverse;
	EquivalenceClassIndex minimalForcedOnBelow;
	EquivalenceClassIndex minimalForcedOffAbove;
	valueInt value;
	countInt count;

};

typedef BakedEquivalenceClassMap<EquivalenceClassInfo> EquivalenceMap;
typedef BakedValuedEquivalenceClass<EquivalenceClassInfo> EquivalenceNode;


class LayerDecomposition {
	std::vector<EquivalenceMap> equivalenceClasses;
public:
	

	LayerDecomposition() = default;
	LayerDecomposition(const FullLayer& fullLayer);

	inline const EquivalenceMap& operator[](size_t i) const {
		return equivalenceClasses[i];
	}
	inline EquivalenceMap& operator[](size_t i) {
		return equivalenceClasses[i];
	}

	auto begin() { return equivalenceClasses.begin(); }
	auto begin() const { return equivalenceClasses.begin(); }
	auto end() { return equivalenceClasses.end(); }
	auto end() const { return equivalenceClasses.end(); }

	EquivalenceNode& empty() { assert(equivalenceClasses.front().size() == 1); return equivalenceClasses.front()[0]; }
	const EquivalenceNode& empty() const { assert(equivalenceClasses.front().size() == 1); return equivalenceClasses.front()[0]; }
	EquivalenceNode& full() { assert(equivalenceClasses.back().size() == 1); return equivalenceClasses.back()[0]; }
	const EquivalenceNode& full() const { assert(equivalenceClasses.back().size() == 1); return equivalenceClasses.back()[0]; }

	size_t getNumberOfFunctionInputs() const {
		return equivalenceClasses.size() - 1;
	}

	inline EquivalenceClassIndex indexOf(const PreprocessedFunctionInputSet& prep) const {
		size_t subLayer = prep.functionInputSet.size();
		size_t indexInSubLayer = equivalenceClasses[subLayer].indexOf(prep);

		return EquivalenceClassIndex(int(subLayer), int(indexInSubLayer));
	}
};
