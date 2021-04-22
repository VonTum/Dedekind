#pragma once

#include <random>
#include "bitSet.h"
#include "booleanFunction.h"
#include "interval.h"
#include "funcTypes.h"
#include "u192.h"

inline bool genBool() {
	return rand() % 2 == 1;
}

inline int generateInt(int max) {
	return rand() % max;
}
inline size_t generateSize_t(size_t max) {
	return rand() % max;
}
inline uint64_t genU64() {
	uint64_t result = 0;
	for(int iter = 0; iter < 5; iter++) {
		result <<= 15;
		result ^= uint64_t(rand());
	}
	return result;
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
BooleanFunction<Variables> generateBF() {
	return BooleanFunction<Variables>(generateBitSet<(1 << Variables)>());
}

template<unsigned int Variables>
BooleanFunction<Variables> generateMBF() {
	BooleanFunction<Variables> result = BooleanFunction<Variables>::empty();

	unsigned int numberOfSeeds = Variables * 2;

	for(unsigned int i = 0; i < numberOfSeeds; i++) {
		result.bitset.set(rand() % (1 << Variables));
	}

	for(unsigned int i = 0; i < Variables + 1; i++) {
		result = result.pred() | result;
	}

	return result;
}

template<unsigned int Variables>
Monotonic<Variables> generateMonotonic() {
	return Monotonic<Variables>(generateMBF<Variables>());
}

template<unsigned int Variables>
BooleanFunction<Variables> generateLayer(unsigned int layer) {
	BooleanFunction<Variables> result = generateBF<Variables>();

	result &= BooleanFunction<Variables>::layerMask(layer);

	return result;
}

template<unsigned int Variables>
Interval<Variables> generateInterval() {
	Monotonic<Variables> mbf = generateMonotonic<Variables>();
	
	// should have a reasonable chance of finding an mbf that is above or below it
	while(true) {
		Monotonic<Variables> secondMBF = generateMonotonic<Variables>();
		
		if(secondMBF <= mbf) {
			return Interval<Variables>(secondMBF, mbf);
		}
		if(mbf <= secondMBF) {
			return Interval<Variables>(mbf, secondMBF);
		}
	}
}

inline u128 genU128() {
#ifdef _MSC_VER
	return u128(genU64(), genU64());
#else
	return __uint128_t(genU64()) | (__uint128_t(genU64()) << 64);
#endif
}

inline u192 genU192() {
	return u192(genU64(), genU64(), genU64());
}
