#include "valuedDecomposition.h"
#include "dedekindDecomposition.h"

#include <iostream>

static void assignInitialValues(LayerDecomposition<ValueCounted>& initialLayer) {
	(*initialLayer[0].begin()).value = 1;
	(*initialLayer[1].begin()).value = 1;
}

static valueInt getTotalValueForLayer(const LayerDecomposition<ValueCounted>& eqClassesOfLayer) {
	valueInt totalValueForFullLayer = 0;

	for(const BakedEquivalenceClassMap<EquivalenceClassInfo<ValueCounted>>& eqMap : eqClassesOfLayer) {
		for(const BakedEquivalenceClass<EquivalenceClassInfo<ValueCounted>>& classOfPrevLayer : eqMap) {
			totalValueForFullLayer += classOfPrevLayer.count * classOfPrevLayer.value;
		}
	}

	return totalValueForFullLayer;
}

template<size_t Size>
static FunctionInputSet computeAvailableElements(const std::bitset<Size>& offInputSet, const FullLayer& curLayer, const FullLayer& layerAbove) {
	FunctionInputSet onInputSet = invert(offInputSet, curLayer);
	return removeForcedOn(layerAbove, onInputSet);
}

static valueInt computeValueOfClass(const BakedEquivalenceClass<EquivalenceClassInfo<ValueCounted>>& availableOptions, const LayerDecomposition<ValueCounted>& decomposition) {
	valueInt totalOptions = 0; // 1 for everything on, no further options

	/*forEachSubsetOfInputSet(availableOptions, decomposition, [&totalOptions](const FunctionInputSet& subGroup, valueInt subGroupValue, countInt occurenceCount) {
		totalOptions += subGroupValue * occurenceCount;
	});*/

	decomposition.forEachSubClassOfClass(availableOptions, [&totalOptions](const BakedEquivalenceClass<EquivalenceClassInfo<ValueCounted>>& cl, countInt occurences) {
		totalOptions += occurences * cl.value;
	});

	return totalOptions;
}

static void computeNextLayerValues(const LayerDecomposition<ValueCounted>& prevLayer, LayerDecomposition<ValueCounted>& curLayer) {
	// resulting values of empty set, and full set
	curLayer.empty().value = 1;
	curLayer.full().value = getTotalValueForLayer(prevLayer);

	// for all other group sizes between the empty set and the full set
	for(size_t setSize = 1; setSize < curLayer.getNumberOfFunctionInputs(); setSize++) {
		std::cout << "Assigning values of " << setSize << "/" << curLayer.getNumberOfFunctionInputs() << "\n";
		for(BakedEquivalenceClass<EquivalenceClassInfo<ValueCounted>>& curClass : curLayer[setSize]) {
			const BakedEquivalenceClass<EquivalenceClassInfo<ValueCounted>>& aboveItem = prevLayer[curClass.minimalForcedOffAbove];

			valueInt valueOfSG = computeValueOfClass(aboveItem, prevLayer);

			curClass.value = valueOfSG;
		}
	}
}

void assignValues(DedekindDecomposition<ValueCounted>& decomp) {
	
}
