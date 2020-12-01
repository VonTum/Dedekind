#pragma once

#include "equivalenceClass.h"
#include "equivalenceClassMap.h"

#include "heapArray.h"

#include <vector>


typedef uint64_t bigInt;
typedef bigInt countInt;
typedef bigInt valueInt;

struct EquivalenceClassIndex {
	int size : 8;
	int indexInSubLayer : 24;
	EquivalenceClassIndex() : size(-1), indexInSubLayer(-1) {}
	EquivalenceClassIndex(size_t size, size_t indexInSubLayer) : size(size), indexInSubLayer(indexInSubLayer) {
		assert(indexInSubLayer >= 0 && indexInSubLayer < (1 << 24));
		assert(size >= 0 && size < 256);
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

	LayerDecomposition(LayerDecomposition&&) = default;
	LayerDecomposition& operator=(LayerDecomposition&&) = default;
	LayerDecomposition(const LayerDecomposition&) = delete;
	LayerDecomposition& operator=(const LayerDecomposition&) = delete;

	inline const EquivalenceMap& operator[](size_t i) const {
		return equivalenceClasses[i];
	}
	inline EquivalenceMap& operator[](size_t i) {
		return equivalenceClasses[i];
	}
	inline EquivalenceNode& operator[](EquivalenceClassIndex index) {
		EquivalenceMap& map = equivalenceClasses[index.size];
		return map[index.indexInSubLayer];
	}
	inline const EquivalenceNode& operator[](EquivalenceClassIndex index) const {
		const EquivalenceMap& map = equivalenceClasses[index.size];
		return map[index.indexInSubLayer];
	}

	inline auto begin() { return equivalenceClasses.begin(); }
	inline auto begin() const { return equivalenceClasses.begin(); }
	inline auto end() { return equivalenceClasses.end(); }
	inline auto end() const { return equivalenceClasses.end(); }

	inline EquivalenceNode& empty() { assert(equivalenceClasses.front().size() == 1); return equivalenceClasses.front()[0]; }
	inline const EquivalenceNode& empty() const { assert(equivalenceClasses.front().size() == 1); return equivalenceClasses.front()[0]; }
	inline EquivalenceNode& full() { assert(equivalenceClasses.back().size() == 1); return equivalenceClasses.back()[0]; }
	inline const EquivalenceNode& full() const { assert(equivalenceClasses.back().size() == 1); return equivalenceClasses.back()[0]; }

	inline size_t getNumberOfFunctionInputs() const { return equivalenceClasses.size() - 1; }

	inline EquivalenceClassIndex indexOf(const PreprocessedFunctionInputSet& prep) const {
		size_t subLayer = prep.functionInputSet.size();
		size_t indexInSubLayer = equivalenceClasses[subLayer].indexOf(prep);

		return EquivalenceClassIndex(subLayer, indexInSubLayer);
	}

	inline EquivalenceNode& inverseOf(const EquivalenceNode& eq) {
		int sizeOfInverse = this->getNumberOfFunctionInputs() - eq.eqClass.size();
		return this->equivalenceClasses[sizeOfInverse][eq.value.inverse];
	}
	inline const EquivalenceNode& inverseOf(const EquivalenceNode& eq) const {
		int sizeOfInverse = this->getNumberOfFunctionInputs() - eq.eqClass.size();
		return this->equivalenceClasses[sizeOfInverse][eq.value.inverse];
	}
};
