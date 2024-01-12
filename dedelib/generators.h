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



template<unsigned int Variables, typename RandomEngine>
void permuteRandom(BooleanFunction<Variables>& bf, RandomEngine& generator, unsigned int from = 0, unsigned int to = Variables) {
	for(unsigned int i = from; i < to; i++) {
		unsigned int selectedIndex = std::uniform_int_distribution<unsigned int>(i, to - 1)(generator);
		if(selectedIndex == i) continue; // leave i where it is
		bf.swap(i, selectedIndex); // put selectedIndex in position i
	}
}

// Doesn't seem to be faster :(
/*template<unsigned int Variables, typename RandomEngine>
void permuteRandomFast(BooleanFunction<Variables>& bf, RandomEngine& generator) {
	if constexpr(Variables == 7) {
		// Only do a single rand call
		uint16_t selectedIndex = std::uniform_int_distribution<uint16_t>(0, 5039)(generator);
		// Efficient divisions and mods by constants
		uint16_t s6 = selectedIndex % 7;
		bf.swap(6, s6);
		selectedIndex /= 7;
		uint16_t s5 = selectedIndex % 6;
		bf.swap(5, s5);
		selectedIndex /= 6;
		uint16_t s4 = selectedIndex % 5;
		bf.swap(4, s4);
		selectedIndex /= 5;
		uint16_t s3 = selectedIndex % 4;
		bf.swap(3, s3);
		selectedIndex /= 4;
		uint16_t s2 = selectedIndex % 3;
		bf.swap(2, s2);
		selectedIndex /= 3;
		uint16_t s1 = selectedIndex % 2;
		bf.swap(1, s1);
		selectedIndex /= 2;
		uint16_t s0 = selectedIndex;
		bf.swap(0, s0);
	} else {
		for(unsigned int i = 0; i < Variables; i++) {
			unsigned int selectedIndex = std::uniform_int_distribution<unsigned int>(i, Variables - 1)(generator);
			if(selectedIndex == i) continue; // leave i where it is
			bf.swap(i, selectedIndex); // put selectedIndex in position i
		}
	}
}*/

template<unsigned int Variables, typename RandomEngine>
void permuteRandom(AntiChain<Variables>& ac, RandomEngine& generator, unsigned int from = 0, unsigned int to = Variables) {
	permuteRandom<Variables, RandomEngine>(ac.bf, generator, from, to);
}

template<unsigned int Variables, typename RandomEngine>
void permuteRandom(Monotonic<Variables>& mbf, RandomEngine& generator, unsigned int from = 0, unsigned int to = Variables) {
	permuteRandom<Variables, RandomEngine>(mbf.bf, generator, from, to);
}

template<unsigned int Variables, typename RandomEngine>
void permuteRandom(Layer<Variables>& layer, RandomEngine& generator, unsigned int from = 0, unsigned int to = Variables) {
	permuteRandom<Variables, RandomEngine>(layer.bf, generator, from, to);
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
