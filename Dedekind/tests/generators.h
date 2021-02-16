#pragma once

#include <random>
#include "../bitSet.h"
#include "../functionInputBitSet.h"
#include "../interval.h"

inline bool genBool() {
	return rand() % 2 == 1;
}

template<size_t Size>
BitSet<Size> generateBitSet() {
	BitSet<Size> bs;

	for(size_t i = 0; i < Size; i++) {
		if(genBool()) {
			bs.set(i);
		}
	}

	return bs;
}

template<unsigned int Variables>
FunctionInputBitSet<Variables> generateFibs() {
	return FunctionInputBitSet<Variables>(generateBitSet<(1 << Variables)>());
}

template<unsigned int Variables>
FunctionInputBitSet<Variables> generateMBF() {
	FunctionInputBitSet<Variables> result = FunctionInputBitSet<Variables>::empty();

	unsigned int numberOfSeeds = rand() % (Variables * 2);

	for(unsigned int i = 0; i < numberOfSeeds; i++) {
		result.bitset.set(rand() % (1 << Variables));
	}

	for(unsigned int i = 0; i < Variables + 1; i++) {
		result = result.prev() | result;
	}

	return result;
}

template<unsigned int Variables>
FunctionInputBitSet<Variables> generateLayer(unsigned int layer) {
	FunctionInputBitSet<Variables> result = generateFibs<Variables>();

	result &= FunctionInputBitSet<Variables>::layerMask(layer);

	return result;
}

template<unsigned int Variables>
Interval<Variables> generateInterval() {
	FunctionInputBitSet<Variables> mbf = generateMBF<Variables>();
	
	// should have a reasonable chance of finding an mbf that is above or below it
	while(true) {
		FunctionInputBitSet<Variables> secondMBF = generateMBF<Variables>();
		
		if(secondMBF.isSubSetOf(mbf)) {
			return Interval<Variables>(secondMBF, mbf);
		}
		if(mbf.isSubSetOf(secondMBF)) {
			return Interval<Variables>(mbf, secondMBF);
		}
	}
}

