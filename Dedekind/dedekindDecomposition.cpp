#include "dedekindDecomposition.h"

#include <iostream>

static void assignInitialValues(LayerDecomposition& initialLayer) {
	(*initialLayer[0].begin()).value.value = 1;
	(*initialLayer[1].begin()).value.value = 1;
}

static valueInt getTotalValueForLayer(const LayerDecomposition& eqClassesOfLayer) {
	valueInt totalValueForFullLayer = 0;

	for(const BakedEquivalenceClassMap<EquivalenceClassInfo>& eqMap : eqClassesOfLayer) {
		for(const BakedValuedEquivalenceClass<EquivalenceClassInfo>& classOfPrevLayer : eqMap) {
			const EquivalenceClassInfo& info = classOfPrevLayer.value;
			totalValueForFullLayer += info.count * info.value;
		}
	}

	return totalValueForFullLayer;
}

static void computeInterLayerConnections(LayerDecomposition& largerLayer, LayerDecomposition& smallerLayer, const FullLayer& fullLargerLayer, const FullLayer& fullSmallerLayer) {
	for(size_t setSize = 0; setSize <= smallerLayer.getNumberOfFunctionInputs(); setSize++) {
		for(BakedValuedEquivalenceClass<EquivalenceClassInfo>& smallerClass : smallerLayer[setSize]) {
			EquivalenceClassIndex index = smallerLayer.indexOf(preprocess(removeForcedOn(fullLargerLayer, smallerClass.eqClass.asFunctionInputSet())));
			smallerClass.value.minimalForcedOffAbove = index;
		}
	}
	for(size_t setSize = 0; setSize <= largerLayer.getNumberOfFunctionInputs(); setSize++) {
		for(BakedValuedEquivalenceClass<EquivalenceClassInfo>& largerClass : largerLayer[setSize]) {
			EquivalenceClassIndex index = largerLayer.indexOf(preprocess(removeForcedOff(fullSmallerLayer, largerClass.eqClass.asFunctionInputSet())));
			largerClass.value.minimalForcedOnBelow = index;
		}
	}


}

template<size_t Size>
static FunctionInputSet computeAvailableElements(const std::bitset<Size>& offInputSet, const FullLayer& curLayer, const FullLayer& layerAbove) {
	FunctionInputSet onInputSet = invert(offInputSet, curLayer);
	return removeForcedOn(layerAbove, onInputSet);
}

static void computeNextLayerValues(const LayerDecomposition& prevLayer, LayerDecomposition& curLayer) {
	LayerDecomposition decomposition(curLayer);
	// resulting values of empty set, and full set
	decomposition.empty().value.value = 1;
	decomposition.full().value.value = getTotalValueForLayer(prevLayer);

	// for all other group sizes between the empty set and the full set
	for(size_t setSize = 1; setSize < curLayer.getNumberOfFunctionInputs(); setSize++) {
		std::cout << "Assigning values of " << setSize << "/" << curLayer.getNumberOfFunctionInputs() << "\n";
		for(BakedValuedEquivalenceClass<EquivalenceClassInfo>& curClass : decomposition[setSize]) {

			BakedValuedEquivalenceClass<EquivalenceClassInfo>& maximalAbove = curClass.value.maximalForcedOnSub

			FunctionInputSet availableElementsInPrevLayer = computeAvailableElements(curClass.eqClass.functionInputSet, curLayer, prevLayer);

			valueInt valueOfSG = computeValueOfClass(availableElementsInPrevLayer, resultOfPrevLayer);

			curClass.value.value = valueOfSG;
		}
	}
}

DedekindDecomposition::DedekindDecomposition(size_t order) : layers(order + 1), layerStack(generateLayers(order)) {
	new(&layers[order]) LayerDecomposition(layerStack.layers[order]);
	assignInitialValues(layers[order]);
	for(size_t i = order; i > 0; i--) {
		new(&layers[i-1]) LayerDecomposition(layerStack.layers[i-1]);
		std::cout << "Linking layers " << i << " - " << (i-1) << "\n";
		computeInterLayerConnections(layers[i], layers[i - 1], layerStack.layers[i], layerStack.layers[i - 1]);
		computeNextLayerValues(layers[i], layers[i-1]);
	}
}
