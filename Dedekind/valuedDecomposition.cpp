#include "valuedDecomposition.h"
#include "dedekindDecomposition.h"

#include "parallelIter.h"

#include <iostream>



static void assignInitialCounts(LayerDecomposition<ValueCounted>& initialLayer) {
	(*initialLayer[0].begin()).count = 1;
	(*initialLayer[1].begin()).count = 1;
}

static void computeLayerCounts(LayerDecomposition<ValueCounted>& curLayer) {
	for(BakedEquivalenceClassMap<EquivalenceClassInfo<ValueCounted>>& subSize : curLayer) {
		for(BakedEquivalenceClass<EquivalenceClassInfo<ValueCounted>>& classOfCurLayer : subSize) {
			classOfCurLayer.count = 0;
		}
	}
	(*curLayer[0].begin()).count = 1;
	for(size_t curSize = 0; curSize < curLayer.getNumberOfFunctionInputs(); curSize++) {
		for(const BakedEquivalenceClass<EquivalenceClassInfo<ValueCounted>>& classOfCurSize : curLayer[curSize]) {
			for(const NextClass& nx : classOfCurSize.iterNextClasses()) {
				curLayer[curSize + 1][nx.nodeIndex].count += classOfCurSize.count * nx.formationCount;
			}
		}
		for(BakedEquivalenceClass<EquivalenceClassInfo<ValueCounted>>& classOfNextSize : curLayer[curSize + 1]) {
			classOfNextSize.count /= curSize + 1;
		}
	}
}

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

template<typename ExtraInfo, typename Func>
static void forEachSubsetOfInputSet(const FunctionInputSet& availableOptions, const LayerDecomposition<ExtraInfo>& eqClasses, Func func) {
	for(size_t offCount = 0; offCount <= availableOptions.size(); offCount++) {
		forEachSubgroup(availableOptions, offCount, [&eqClassesOffCnt = eqClasses[offCount], &func](const FunctionInputSet& subGroup) {
			PreprocessedFunctionInputSet preprocessed = preprocess(subGroup);
			func(subGroup, eqClassesOffCnt.get(preprocessed).value);
		});
	}
}

//#include "toString.h"
//#include <iostream>

static void computeNextLayerValues(const LayerDecomposition<ValueCounted>& prevLayer, LayerDecomposition<ValueCounted>& curLayer) {
	// resulting values of empty set, and full set
	curLayer.empty().value = 1;
	curLayer.full().value = getTotalValueForLayer(prevLayer);

	// for all other group sizes between the empty set and the full set
	for(size_t setSize = 1; setSize < curLayer.getNumberOfFunctionInputs(); setSize++) {
		std::cout << "Assigning values of " << setSize << "/" << curLayer.getNumberOfFunctionInputs() << "\n";
		iterCollectionInParallelWithPerThreadBuffer(curLayer[setSize], [&prevLayer]() {return prevLayer.makeForEachBuffer(); },
			[&prevLayer](BakedEquivalenceClass<EquivalenceClassInfo<ValueCounted>>& curClass, LayerDecomposition<ValueCounted>::ForEachBuffer& buf) {
				const BakedEquivalenceClass<EquivalenceClassInfo<ValueCounted>>& aboveItem = prevLayer[curClass.minimalForcedOffAbove];

				valueInt valueOfSG = aboveItem.value;
				prevLayer.forEachSubClassOfClass(aboveItem, buf, [&valueOfSG](const BakedEquivalenceClass<EquivalenceClassInfo<ValueCounted>>& cl, countInt occurences) {
					valueOfSG += occurences * cl.value;
				});

				curClass.value = valueOfSG;
			}
		);
	}
}

void ValueCounted::populate(DedekindDecomposition<ValueCounted>& decomp) {
	std::cout << "Assigning initial values for layer " << decomp.numLayers() - 1 << "\n";
	assignInitialCounts(decomp[decomp.numLayers() - 1]);
	assignInitialValues(decomp[decomp.numLayers() - 1]);

	for(size_t i = decomp.numLayers() - 1; i > 0; i--) {
		std::cout << "Computing values for " << (i - 1) << " from " << i << "\n";
		computeLayerCounts(decomp[i - 1]);
		computeNextLayerValues(decomp[i], decomp[i - 1]);
	}
}
valueInt ValueCounted::getDedekind(const DedekindDecomposition<ValueCounted>& decomp) {
	return decomp.fullBottom().value + 1;
}
