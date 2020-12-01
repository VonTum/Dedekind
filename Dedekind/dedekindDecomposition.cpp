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

static void computeInterLayerConnections(LayerDecomposition& largerClasses, LayerDecomposition& smallerClasses, const FullLayer& fullLargerLayer, const FullLayer& fullSmallerLayer) {
	for(size_t setSize = 0; setSize <= smallerClasses.getNumberOfFunctionInputs(); setSize++) {
		for(BakedValuedEquivalenceClass<EquivalenceClassInfo>& smallerClass : smallerClasses[setSize]) {
			PreprocessedFunctionInputSet preprocessed = preprocess(removeForcedOff(fullLargerLayer, smallerClass.eqClass.asFunctionInputSet()));
			EquivalenceClassIndex index = largerClasses.indexOf(preprocessed);
			smallerClass.value.minimalForcedOffAbove = index;
		}
	}
	for(size_t setSize = 0; setSize <= largerClasses.getNumberOfFunctionInputs(); setSize++) {
		for(BakedValuedEquivalenceClass<EquivalenceClassInfo>& largerClass : largerClasses[setSize]) {
			PreprocessedFunctionInputSet preprocessed = preprocess(removeForcedOn(fullSmallerLayer, largerClass.eqClass.asFunctionInputSet()));
			EquivalenceClassIndex index = smallerClasses.indexOf(preprocessed);
			largerClass.value.minimalForcedOnBelow = index;
		}
	}


}

template<size_t Size>
static FunctionInputSet computeAvailableElements(const std::bitset<Size>& offInputSet, const FullLayer& curLayer, const FullLayer& layerAbove) {
	FunctionInputSet onInputSet = invert(offInputSet, curLayer);
	return removeForcedOn(layerAbove, onInputSet);
}

template<typename Func>
static void forEachSubsetOfInputSet(const FunctionInputSet& availableOptions, const LayerDecomposition& eqClasses, Func func) {
	for(size_t offCount = 0; offCount <= availableOptions.size(); offCount++) {
		forEachSubgroup(availableOptions, offCount, [&eqClassesOffCnt = eqClasses[offCount], &func](const FunctionInputSet& subGroup) {
			PreprocessedFunctionInputSet preprocessed = preprocess(subGroup);
			func(subGroup, eqClassesOffCnt.get(preprocessed).value.value, 1);
		});
	}
}

static valueInt computeValueOfClass(const FunctionInputSet& availableOptions, const LayerDecomposition& decomposition) {
	valueInt totalOptions = 0; // 1 for everything on, no further options

	forEachSubsetOfInputSet(availableOptions, decomposition, [&totalOptions](const FunctionInputSet& subGroup, valueInt subGroupValue, countInt occurenceCount) {
		totalOptions += subGroupValue * occurenceCount;
	});

	return totalOptions;
}

static void computeNextLayerValues(const LayerDecomposition& prevLayer, LayerDecomposition& curLayer) {
	// resulting values of empty set, and full set
	curLayer.empty().value.value = 1;
	curLayer.full().value.value = getTotalValueForLayer(prevLayer);

	// for all other group sizes between the empty set and the full set
	for(size_t setSize = 1; setSize < curLayer.getNumberOfFunctionInputs(); setSize++) {
		std::cout << "Assigning values of " << setSize << "/" << curLayer.getNumberOfFunctionInputs() << "\n";
		for(BakedValuedEquivalenceClass<EquivalenceClassInfo>& curClass : curLayer[setSize]) {
			const BakedValuedEquivalenceClass<EquivalenceClassInfo>& aboveItem = prevLayer[curClass.value.minimalForcedOffAbove];

			valueInt valueOfSG = computeValueOfClass(aboveItem.eqClass.asFunctionInputSet(), prevLayer);

			curClass.value.value = valueOfSG;
		}
	}
}

DedekindDecomposition::DedekindDecomposition(size_t order) : layers(order + 1), layerStack(generateLayers(order)) {
	std::cout << "Generating decomposition for layer " << order << "\n";
	new(&layers[order]) LayerDecomposition(layerStack.layers[order]);
	std::cout << "Assigning initial values\n";
	assignInitialValues(layers[order]);
	for(size_t i = order; i > 0; i--) {
		std::cout << "Generating decomposition for layer " << i - 1 << "\n";
		new(&layers[i-1]) LayerDecomposition(layerStack.layers[i-1]);
		std::cout << "Linking layers " << i << " - " << (i-1) << "\n";
		computeInterLayerConnections(layers[i], layers[i - 1], layerStack.layers[i], layerStack.layers[i - 1]);
		computeNextLayerValues(layers[i], layers[i-1]);
	}
}
