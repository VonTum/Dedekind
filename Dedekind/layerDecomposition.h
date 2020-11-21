#pragma once

#include "equivalenceClass.h"
#include "equivalenceClassMap.h"

#include <vector>


typedef uint64_t bigInt;
typedef bigInt countInt;
typedef bigInt valueInt;

struct EquivalenceClassInfo {
	struct AdjacentClass {
		ValuedEquivalenceClass<EquivalenceClassInfo>* node;
		countInt formationCount;
	};

	countInt count;
	std::vector<AdjacentClass> superClasses;
	std::vector<AdjacentClass> subClasses;

	valueInt value;
};

class LayerDecomposition {
	std::vector<EquivalenceClassMap<EquivalenceClassInfo>> equivalenceClasses;
public:
	LayerDecomposition(const FullLayer& fullLayer);

	inline const EquivalenceClassMap<EquivalenceClassInfo>& operator[](size_t i) const {
		return equivalenceClasses[i];
	}
	inline EquivalenceClassMap<EquivalenceClassInfo>& operator[](size_t i) {
		return equivalenceClasses[i];
	}

	auto begin() { return equivalenceClasses.begin(); }
	auto begin() const { return equivalenceClasses.begin(); }
	auto end() { return equivalenceClasses.end(); }
	auto end() const { return equivalenceClasses.end(); }
};
