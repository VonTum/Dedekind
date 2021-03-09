#pragma once

#include <cstdint>
#include "funcTypes.h"
#include <iostream>

template<unsigned int Variables>
void printSet(const BooleanFunction<Variables>& bf) {
	std::cout << "{";
	bf.forEachOne([](size_t index) {
		std::cout << FunctionInput{static_cast<unsigned int>(index)} << ", ";
	});
	std::cout << "}\n";
}

template<unsigned int Variables>
uint64_t getNumChoicesRecursiveDown(const BooleanFunction<Variables>& chooseableMask) {
	using MBF = Monotonic<Variables>;
	using BF = BooleanFunction<Variables>;
	using AC = AntiChain<Variables>;
	using LR = Layer<Variables>;
	
	size_t numChooseableElements = chooseableMask.size();
	if(numChooseableElements == 0) {
		//printSet(chosen1);
		return 1;
	} else if(numChooseableElements == 1) {
		return 2;
	}

	int topLayerIndex = Variables;
	BF layerMask;
	BF topLayer;
	for(; ; topLayerIndex--) {
		layerMask = BF(BF::layerMask(topLayerIndex));
		topLayer = chooseableMask & layerMask;
		if(!topLayer.isEmpty()) {
			break;
		}
	}

	if(topLayer == chooseableMask) {
		// single layer left
		return 1 << numChooseableElements;
	}

	BF furtherChoices = chooseableMask & ~layerMask;
	assert(furtherChoices.isSubSetOf(layerMask.pred().monotonizeDown()));

	uint64_t numChoices = 0;
	topLayer.forEachSubSet([&](const BF& chosenOn) {
		BF forcedElements = chosenOn.monotonizeDown();

		BF newChooseable = andnot(furtherChoices, forcedElements);

		uint64_t subChoices = getNumChoicesRecursiveDown(newChooseable);
		numChoices += subChoices;
	});

	return numChoices;
}

template<unsigned int Variables>
uint64_t getNumChoices(const BooleanFunction<Variables>& chooseableMask) {
	return getNumChoicesRecursiveDown(chooseableMask);
}

template<unsigned int Variables>
uint64_t computeIntervalSizeExtention(const Monotonic<Variables>& prevIntervalTop, uint64_t prevIntervalSize, size_t extention) {
	using MBF = Monotonic<Variables>;
	using BF = BooleanFunction<Variables>;
	using AC = AntiChain<Variables>;

	assert(prevIntervalTop.succ().bf.contains(extention));
	assert(!prevIntervalTop.bf.contains(extention));

	// new size is the current size, plus all functions that do contain the new extention
	// all extra functions *must* contain the extention
	// therefore all these elements will be fixed on
	// only need to iterate over all non-forced elements

	MBF forcedElements = AC{extention}.asMonotonic();
	BF chooseableElements = andnot(prevIntervalTop.bf, forcedElements.bf);

	if(chooseableElements.isEmpty()) {
		return prevIntervalSize + 1;
	}

	return prevIntervalSize + getNumChoices(chooseableElements);

	throw "unreachable";
}


