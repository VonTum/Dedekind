#include "intervalDecomposition.h"

#include <map>

#include <iostream>
#include "toString.h"

static valueInt getNumberOfOptionsInSubLayers(const DedekindDecomposition<IntervalSize>& decomp, size_t curLayerIndex, const EqClass<IntervalSize>& optionsEqClass, const EqClass<IntervalSize>& blockedElements) {
	const LayerDecomposition<IntervalSize>& curLayer = decomp[curLayerIndex];


}

using AvailableChoices = SmallVector<EquivalenceClassIndex, MAX_PREPROCESSED + 1>;

struct EquivClassIndexCmp {
	bool operator()(const AvailableChoices& a, const AvailableChoices& b) const {
		assert(a.size() == b.size());

		for(size_t i = 0; i < a.size(); i++) {
			if(a[i] != b[i]) {
				return a[i] < b[i];
			}
		}
		return false;
	}
};
using AvailableChoiceMap = std::map<AvailableChoices, valueInt, EquivClassIndexCmp()>;

static valueInt getRecursiveChoices(const SmallVector<FunctionInputSet, MAX_PREPROCESSED + 1>& baseNonDominatedLayers) {
	valueInt totalChoices = 0;

	if(baseNonDominatedLayers.size() == 0) {
		return 1;
	} else {
		//std::cout << baseNonDominatedLayers.back() << ":\n";
		forEachSubgroup(baseNonDominatedLayers.back(), [&baseNonDominatedLayers, &totalChoices](const FunctionInputSet& forcingElems) {
			//std::cout << "    " << forcingElems << "\n";
			// the forcing elements are elements that force the elements from the layer below. For example, for monotonic boolean functions going false->true, this would be the false-elements. 
			SmallVector<FunctionInputSet, MAX_PREPROCESSED + 1> availableInThisSplit(baseNonDominatedLayers.size() - 1);
			for(size_t i = 0; i < baseNonDominatedLayers.size() - 1; i++) {
				availableInThisSplit[i] = getNonDominatedElements(baseNonDominatedLayers[i], forcingElems);
			}
			totalChoices += getRecursiveChoices(availableInThisSplit);
		});
		return totalChoices;
	}
}

static valueInt computeIntervalSizeToBottom(const DedekindDecomposition<IntervalSize>& decomp, size_t curLayerIndex, const EqClass<IntervalSize>& currentlyFindingIntervalSizeFor) {
	const LayerDecomposition<IntervalSize>& curLayer = decomp[curLayerIndex];

	FunctionInputSet fis = currentlyFindingIntervalSizeFor.eqClass.asFunctionInputSet();
	SmallVector<FunctionInputSet, MAX_PREPROCESSED + 1> forcedInPrevLayers(curLayerIndex+1);
	for(size_t i = 0; i < curLayerIndex; i++) {
		forcedInPrevLayers[i] = getDominatedElements(decomp.layer(i), fis);
	}
	forcedInPrevLayers[curLayerIndex] = std::move(fis);
	return getRecursiveChoices(forcedInPrevLayers);
}

/*static AvailableChoiceMap computeInitialAvailableChoiceMap() {
	AvailableChoiceMap result;
	AvailableChoices empty(1, EquivalenceClassIndex(0, 0));
	result.insert(std::make_pair(empty, 0));
	AvailableChoices full(1, EquivalenceClassIndex(1, 0));
	result.insert(std::make_pair(full, 1));
}*/

void assignIntervalSizes(DedekindDecomposition<IntervalSize>& decomp) {
	//AvailableChoiceMap curAvailableChoiceMap = computeInitialAvailableChoiceMap();
	for(size_t curLayerIndex = 0; curLayerIndex < decomp.numLayers(); curLayerIndex++) {
		LayerDecomposition<IntervalSize>& curLayer = decomp[curLayerIndex];
		for(size_t setSize = 1; setSize <= curLayer.getNumberOfFunctionInputs(); setSize++) {
			std::cout << "Assigning interval sizes of " << setSize << "/" << curLayer.getNumberOfFunctionInputs() << "\n";
			iterCollectionInParallelWithPerThreadBuffer(curLayer[setSize], [&curLayer]() {return curLayer.makeForEachBuffer(); },
														[&](EqClass<IntervalSize>& curClass, LayerDecomposition<IntervalSize>::ForEachBuffer& buf) {

				//std::cout << curClass.eqClass.asFunctionInputSet() << ":\n";
				curClass.intervalSizeToBottom = computeIntervalSizeToBottom(decomp, curLayerIndex, curClass);
				//std::cout << "Size: " << curClass.intervalSizeToBottom << "\n";
			});
		}
	}
}
