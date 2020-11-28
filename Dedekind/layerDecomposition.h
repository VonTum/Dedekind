#pragma once

#include "equivalenceClass.h"
#include "equivalenceClassMap.h"

#include "heapArray.h"

#include <vector>


typedef uint64_t bigInt;
typedef bigInt countInt;
typedef bigInt valueInt;

struct EquivalenceClassInfo {
	class AdjacentClass {
		uint32_t data;
	public:
		AdjacentClass() = default;
		AdjacentClass(size_t nodeIndex, unsigned int formationCount) : data(nodeIndex << 8 | formationCount) {
			assert(nodeIndex < (1 << 24));
			assert(formationCount < 256);
		}

		size_t getNodeIndex() const { return data >> 8; }
		unsigned int getFormationCount() const { return data & 0xFF; }

		AdjacentClass& operator++() {
			++data;
			assert((data & 0xFF) != 0);
			return *this;
		}
	};

	AdjacentClass* extendedClasses;
	size_t numberOfExtendedClasses;
	countInt count;

	valueInt value;
};

class LayerDecomposition {
	std::vector<BakedEquivalenceClassMap<EquivalenceClassInfo>> equivalenceClasses;
public:
	LayerDecomposition() = default;
	LayerDecomposition(const FullLayer& fullLayer);

	inline const BakedEquivalenceClassMap<EquivalenceClassInfo>& operator[](size_t i) const {
		return equivalenceClasses[i];
	}
	inline BakedEquivalenceClassMap<EquivalenceClassInfo>& operator[](size_t i) {
		return equivalenceClasses[i];
	}

	auto begin() { return equivalenceClasses.begin(); }
	auto begin() const { return equivalenceClasses.begin(); }
	auto end() { return equivalenceClasses.end(); }
	auto end() const { return equivalenceClasses.end(); }
};
