#include "intervalDecomposition.h"


#include <iostream>
#include "toString.h"

static valueInt getNumberOfOptionsInSubLayers(const DedekindDecomposition<IntervalSize>& decomp, size_t curLayerIndex, const EqClass<IntervalSize>& optionsEqClass, const EqClass<IntervalSize>& blockedElements) {
	const LayerDecomposition<IntervalSize>& curLayer = decomp[curLayerIndex];


}

static void computeIntervalSizeToBottom(const DedekindDecomposition<IntervalSize>& decomp, size_t curLayerIndex, const EqClass<IntervalSize>& currentlyFindingIntervalSizeFor, LayerDecomposition<IntervalSize>::ForEachBuffer& buf) {
	const LayerDecomposition<IntervalSize>& curLayer = decomp[curLayerIndex];
	const LayerDecomposition<IntervalSize>& prevLayer = decomp[curLayerIndex-1];
	const FullLayer& prevFullLayer = decomp.layer(curLayerIndex - 1);

	FunctionInputSet forcedInPrevLayer = getDominatedElements(prevFullLayer, currentlyFindingIntervalSizeFor.eqClass.asFunctionInputSet());

	// the forcing elements are elements that force the elements from the layer below. For example, for monotonic boolean functions going false->true, this would be the false-elements. 
	forEachSplit(currentlyFindingIntervalSizeFor.eqClass.asFunctionInputSet(), [&prevFullLayer, &forcedInPrevLayer](const FunctionInputSet& forcingElems, const FunctionInputSet& nonforcingElems) {
		FunctionInputSet forcedInThisSplit = getDominatedElements(prevFullLayer, forcingElems);

	});
}

void assignIntervalSizes(DedekindDecomposition<IntervalSize>& decomp) {
	for(size_t curLayerIndex = 0; curLayerIndex < decomp.numLayers(); curLayerIndex++) {
		LayerDecomposition<IntervalSize>& curLayer = decomp[curLayerIndex];
		for(size_t setSize = 1; setSize < curLayer.getNumberOfFunctionInputs(); setSize++) {
			std::cout << "Assigning interval sizes of " << setSize << "/" << curLayer.getNumberOfFunctionInputs() << "\n";
			iterCollectionInParallelWithPerThreadBuffer(curLayer[setSize], [&curLayer]() {return curLayer.makeForEachBuffer(); },
														[&decomp, curLayerIndex](EqClass<IntervalSize>& curClass, LayerDecomposition<IntervalSize>::ForEachBuffer& buf) {

				computeIntervalSizeToBottom(decomp, curLayerIndex, curClass, buf);
			});
		}
	}


}
