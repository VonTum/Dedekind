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

// the home stretch, must be *highly* optimized
// this is just for every possible choice in lower layer, find all the elements that are still chooseable in upper layer and popcnt
template<unsigned int Variables>
uint64_t getNumChoicesForLast2Layers(const AntiChain<Variables>& lowerLayer, const AntiChain<Variables>& upperLayer) {
	uint64_t total = 0;

	lowerLayer.forEachSubSet([&](const AntiChain<Variables>& chosenOff) {
		BooleanFunction<Variables> forcedElements = chosenOff.bf.monotonizeUp();

		BooleanFunction<Variables> newChooseable = andnot(upperLayer.bf, forcedElements);

		total += uint64_t(1) << newChooseable.size();
	});

	return total;
}



template<unsigned int Variables>
uint64_t getNumChoicesRecursiveUp(const BooleanFunction<Variables>& chooseableElements, const Monotonic<Variables>& conflictingMask) {
	using MBF = Monotonic<Variables>;
	using BF = BooleanFunction<Variables>;
	using AC = AntiChain<Variables>;
	using LR = Layer<Variables>;

	BF conflictingElements = chooseableElements & conflictingMask.bf;

	// none of the elements conflict with one another
	if(conflictingElements.isEmpty()) {
		return uint64_t(1) << chooseableElements.size();
	}

	int bottomLayerIndex = 0;
	BF bottomLayer;
	for(; ; bottomLayerIndex++) {
		BF layerMask = BF(BF::layerMask(bottomLayerIndex));
		bottomLayer = chooseableElements & layerMask;
		if(!bottomLayer.isEmpty()) {
			break;
		}
	}

	BF furtherChoices = andnot(chooseableElements, bottomLayer);

	// OPTIMISATION, reduces if statements for conflictingElements.isEmpty()
	if((conflictingMask.bf & furtherChoices).isEmpty()) {
		return getNumChoicesForLast2Layers<Variables>(AC(bottomLayer), AC(furtherChoices));
	}

	uint64_t numChoices = 0;
	bottomLayer.forEachSubSet([&](const BF& chosenOff) {
		BF forcedElements = chosenOff.monotonizeUp();

		BF newChooseable = andnot(furtherChoices, forcedElements);

		uint64_t subChoices = getNumChoicesRecursiveUp(newChooseable, conflictingMask);
		numChoices += subChoices;
	});

	return numChoices;
}
/*
template<unsigned int Variables>
AntiChain<Variables> getBiggestAntiChain(const BooleanFunction<Variables>& boolFunc) {
	Monotonic<Variables> monotonizedDown(boolFunc.monotonizeDown());
	AntiChain<Variables> topAC = monotonizedDown.asAntiChain();
	size_t topACSize = topAC.size();
	Layer<Variables> biggestLayer = getBiggestLayer(boolFunc);
	size_t biggestLayerSize = biggestLayer.size();

	AntiChain<Variables> biggestAC = topAC;
	if(biggestLayerSize > topACSize) {
		biggestAC = biggestLayer.asAntiChain();
	}

	return biggestAC;
}
*/
template<unsigned int Variables>
uint64_t getNumChoices(const BooleanFunction<Variables>& chooseableMask) {
	Monotonic<Variables> allBelowTop(chooseableMask.monotonizeDown());
	AntiChain<Variables> nonConflictingTop = allBelowTop.asAntiChain();

	Layer<Variables> topLayer = getTopLayer(chooseableMask);
	BooleanFunction<Variables> shortenedChooseableMask = andnot(chooseableMask, topLayer.bf);

	Monotonic<Variables> allBelowShortenedTop(shortenedChooseableMask.monotonizeDown());
	AntiChain<Variables> nonConflictingTopShortened = allBelowShortenedTop.asAntiChain();

	// OPTIMISATION, biggest possible 2^N
	if(nonConflictingTopShortened.size() > nonConflictingTop.size()) {
		// more optimal option, remove the top layer to get a wider top layer, resulting in bigger shifts, and less overall work
		
		//std::cout << "A";

		size_t total = 0;
		// explicitly make choices for the top layer, so these won't be available to choose anymore
		topLayer.forEachSubSet([&](const Layer<Variables>& chosenOn) {
			// these remove all the choices below the chosenOn elements
			BooleanFunction<Variables> blockedElements = chosenOn.bf.monotonizeDown();
			BooleanFunction<Variables> newChooseable = andnot(shortenedChooseableMask, blockedElements);

			Monotonic<Variables> conflicting(newChooseable.monotonizeDown().pred());
			total += getNumChoicesRecursiveUp(newChooseable, conflicting);
		});
		return total;
	}
	// existing top is the best, continue with it

	//std::cout << "B";
	Monotonic<Variables> conflicting = allBelowTop.pred();
	return getNumChoicesRecursiveUp(chooseableMask, conflicting);
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


