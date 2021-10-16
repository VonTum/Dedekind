#pragma once

#include <cstdint>
#include "funcTypes.h"
#include <iostream>

template<unsigned int Variables>
uint64_t getNumChoicesRecursiveDown(const BooleanFunction<Variables>& chooseableMask) {
	using BF = BooleanFunction<Variables>;
	
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
uint64_t getNumChoicesForLast2LayersFallback(const AntiChain<Variables>& lowerLayer, const AntiChain<Variables>& upperLayer) {
	uint64_t total = 0;

	lowerLayer.forEachSubSet([&](const AntiChain<Variables>& chosenOff) {
		BooleanFunction<Variables> forcedElements = chosenOff.bf.monotonizeUp();

		BooleanFunction<Variables> newChooseable = andnot(upperLayer.bf, forcedElements);

		total += uint64_t(1) << newChooseable.size();
	});

	return total;
}

// the home stretch, must be *highly* optimized
// this is just for every possible choice in lower layer, find all the elements that are still chooseable in upper layer and popcnt
template<>
inline uint64_t getNumChoicesForLast2LayersFallback(const AntiChain<7>& lowerLayer, const AntiChain<7>& upperLayer) {
	uint64_t total = 0;

	uint64_t mask0 = _mm_extract_epi64(lowerLayer.bf.bitset.data, 0);
	uint64_t mask1 = _mm_extract_epi64(lowerLayer.bf.bitset.data, 1);
	uint64_t data1 = mask1;

	while(true) {
		uint64_t data0 = mask0;
		while(true) {
			BooleanFunction<7> chosenOff;
			chosenOff.bitset.data = _mm_set_epi64x(data1, data0);

			BooleanFunction<7> forcedElements = chosenOff.monotonizeUp();
			BooleanFunction<7> newChooseable = andnot(upperLayer.bf, forcedElements);
			total += uint64_t(1) << newChooseable.size();

			if(data0 == 0) break;
			data0--;
			data0 &= mask0;
		}
		if(data1 == 0) break;
		data1--;
		data1 &= mask1;
	}

	return total;
}

/*
* AVX
*/
#ifdef __AVX2__
inline __m256i monotonizeUp6AVX256(__m256i bs) {
	__m256i v0Added = _mm256_slli_epi64(_mm256_andnot_si256(_mm256_set1_epi64x(0xaaaaaaaaaaaaaaaa), bs), 1);
	bs = _mm256_or_si256(bs, v0Added);
	__m256i v1Added = _mm256_slli_epi64(_mm256_andnot_si256(_mm256_set1_epi64x(0xcccccccccccccccc), bs), 2);
	bs = _mm256_or_si256(bs, v1Added);
	__m256i v2Added = _mm256_slli_epi64(_mm256_andnot_si256(_mm256_set1_epi64x(0xF0F0F0F0F0F0F0F0), bs), 4);
	bs = _mm256_or_si256(bs, v2Added);
	__m256i v3Added = _mm256_slli_epi16(bs, 8);
	bs = _mm256_or_si256(bs, v3Added);
	__m256i v4Added = _mm256_slli_epi32(bs, 16);
	bs = _mm256_or_si256(bs, v4Added);
	__m256i v5Added = _mm256_slli_epi64(bs, 32);
	bs = _mm256_or_si256(bs, v5Added);

	return bs;
}

inline __m256i custom_mm256_popcnt_epi64(__m256i values) {
	alignas(32) uint64_t buf[4];
	_mm256_store_si256(reinterpret_cast<__m256i*>(buf), values);
	for(uint64_t& item : buf) {
		item = popcnt64(item);
	}
	return _mm256_load_si256(reinterpret_cast<__m256i*>(buf));;
}

inline uint64_t custom_mm256_hsum_epi64(__m256i values) {
	alignas(32) uint64_t buf[4];
	_mm256_store_si256(reinterpret_cast<__m256i*>(buf), values);
	return buf[0] + buf[1] + buf[2] + buf[3];
}

template<bool IsReverse>
inline uint64_t getNumChoicesLast2LayersAVX256(uint64_t bits0, uint64_t bits1, uint64_t upper0, uint64_t upper1) {
	uint64_t firstBit = uint64_t(1) << ctz64(bits0);
	bits0 &= ~firstBit;
	uint64_t secondBit = uint64_t(1) << ctz64(bits0);
	bits0 &= ~secondBit;


	__m256i upperLayer0 = _mm256_set1_epi64x(upper0);
	__m256i upperLayer1 = _mm256_set1_epi64x(upper1);

	__m256i extraBitsMask0 = _mm256_set_epi64x(0, firstBit, secondBit, firstBit | secondBit);

	__m256i totals = _mm256_setzero_si256();

	uint64_t data1 = bits1;
	while(true) {
		__m256i chosenOff1 = _mm256_set1_epi64x(data1);
		__m256i forcedElements1Pre = monotonizeUp6AVX256(chosenOff1);
		uint64_t data0 = bits0;
		while(true) {
			__m256i chosenOff0 = _mm256_or_si256(_mm256_set1_epi64x(data0), extraBitsMask0);

			__m256i forcedElements0Pre = monotonizeUp6AVX256(chosenOff0);
			__m256i forcedElements0 = IsReverse ? _mm256_or_si256(forcedElements1Pre, forcedElements0Pre) : forcedElements0Pre;
			__m256i forcedElements1 = IsReverse ? forcedElements1Pre : _mm256_or_si256(forcedElements1Pre, forcedElements0Pre);

			__m256i newChooseable0 = _mm256_andnot_si256(forcedElements0, upperLayer0);
			__m256i newChooseable1 = _mm256_andnot_si256(forcedElements1, upperLayer1);

			// can do this because newChooseable0 and newChooseable1 won't have any conflicting elements, since upperLayer is an AntiChain. 
			// if they had an overlapping element, then newChooseable1 would dominate newChooseable0, since it's just the same element with var6 enabled
			__m256i combinedChooseableForCounting = _mm256_or_si256(newChooseable0, newChooseable1);

			// sadly, no popcnt :( It would have been amazingly efficient otherwise
			__m256i counts = custom_mm256_popcnt_epi64(combinedChooseableForCounting);
			__m256i sizes = _mm256_sllv_epi64(_mm256_set1_epi64x(1), counts);

			totals = _mm256_add_epi64(totals, sizes);

			if(data0 == 0) break;
			data0--;
			data0 &= bits0;
		}
		if(data1 == 0) break;
		data1--;
		data1 &= bits1;
	}

	return custom_mm256_hsum_epi64(totals);
}
#endif

/*
* AVX512
*/
#ifdef __AVX512F__
inline __m512i monotonizeUp6AVX512(__m512i bs) {
	__m512i v0Added = _mm512_slli_epi64(_mm512_andnot_si512(_mm512_set1_epi64(0xaaaaaaaaaaaaaaaa), bs), 1);
	bs = _mm512_or_si512(bs, v0Added);
	__m512i v1Added = _mm512_slli_epi64(_mm512_andnot_si512(_mm512_set1_epi64(0xcccccccccccccccc), bs), 2);
	bs = _mm512_or_si512(bs, v1Added);
	__m512i v2Added = _mm512_slli_epi64(_mm512_andnot_si512(_mm512_set1_epi64(0xF0F0F0F0F0F0F0F0), bs), 4);
	bs = _mm512_or_si512(bs, v2Added);
	__m512i v3Added = _mm512_slli_epi16(bs, 8);
	bs = _mm512_or_si512(bs, v3Added);
	__m512i v4Added = _mm512_slli_epi32(bs, 16);
	bs = _mm512_or_si512(bs, v4Added);
	__m512i v5Added = _mm512_slli_epi64(bs, 32);
	bs = _mm512_or_si512(bs, v5Added);

	return bs;
}

inline __m512i custom_mm512_popcnt_epi64(__m512i values) {
	alignas(32) uint64_t buf[8];
	_mm512_store_epi64(buf, values);
	for(uint64_t& item : buf) {
		item = popcnt64(item);
	}
	return _mm512_load_si512(reinterpret_cast<__m512i*>(buf));;
}

inline uint64_t custom_mm512_hsum_epi64(__m512i values) {
	alignas(32) uint64_t buf[8];
	_mm512_store_epi64(buf, values);
	return buf[0] + buf[1] + buf[2] + buf[3] + buf[4] + buf[5] + buf[6] + buf[7];
}

template<bool IsReverse>
inline uint64_t getNumChoicesLast2LayersAVX512(uint64_t bits0, uint64_t bits1, uint64_t upper0, uint64_t upper1) {
	uint64_t firstBit = uint64_t(1) << ctz64(bits0);
	bits0 &= ~firstBit;
	uint64_t secondBit = uint64_t(1) << ctz64(bits0);
	bits0 &= ~secondBit;
	uint64_t thirdBit = uint64_t(1) << ctz64(bits0);
	bits0 &= ~thirdBit;


	__m512i upperLayer0 = _mm512_set1_epi64(upper0);
	__m512i upperLayer1 = _mm512_set1_epi64(upper1);

	__m512i extraBitsMask0 = _mm512_set_epi64(0, firstBit, secondBit, secondBit | firstBit, thirdBit, thirdBit | firstBit, thirdBit | secondBit, thirdBit | secondBit | firstBit);

	__m512i totals = _mm512_setzero_si512();

	uint64_t data1 = bits1;
	while(true) {
		__m512i chosenOff1 = _mm512_set1_epi64(data1);
		__m512i forcedElements1Pre = monotonizeUp6AVX512(chosenOff1);
		uint64_t data0 = bits0;
		while(true) {
			__m512i chosenOff0 = _mm512_or_si512(_mm512_set1_epi64(data0), extraBitsMask0);

			__m512i forcedElements0Pre = monotonizeUp6AVX512(chosenOff0);
			__m512i forcedElements0 = IsReverse ? _mm512_or_si512(forcedElements1Pre, forcedElements0Pre) : forcedElements0Pre;
			__m512i forcedElements1 = IsReverse ? forcedElements1Pre : _mm512_or_si512(forcedElements1Pre, forcedElements0Pre);

			__m512i newChooseable0 = _mm512_andnot_si512(forcedElements0, upperLayer0);
			__m512i newChooseable1 = _mm512_andnot_si512(forcedElements1, upperLayer1);

			// can do this because newChooseable0 and newChooseable1 won't have any conflicting elements, since upperLayer is an AntiChain. 
			// if they had an overlapping element, then newChooseable1 would dominate newChooseable0, since it's just the same element with var6 enabled
			__m512i combinedChooseableForCounting = _mm512_or_si512(newChooseable0, newChooseable1);

			// sadly, no popcnt :( It would have been amazingly efficient otherwise
#ifdef __AVX512VPOPCNTDQ__ 
			__m512i counts = _mm512_popcnt_epi64(combinedChooseableForCounting);
#else
			__m512i counts = custom_mm512_popcnt_epi64(combinedChooseableForCounting);
#endif
			__m512i sizes = _mm512_sllv_epi64(_mm512_set1_epi64(1), counts);

			totals = _mm512_add_epi64(totals, sizes);

			if(data0 == 0) break;
			data0--;
			data0 &= bits0;
		}
		if(data1 == 0) break;
		data1--;
		data1 &= bits1;
	}

	return custom_mm512_hsum_epi64(totals);
}
#endif

/*
* AVX512 END
*/

template<unsigned int Variables>
uint64_t getNumChoicesForLast2Layers(const AntiChain<Variables>& lowerLayer, const AntiChain<Variables>& upperLayer) {
	return getNumChoicesForLast2LayersFallback(lowerLayer, upperLayer);
}

template<>
inline uint64_t getNumChoicesForLast2Layers(const AntiChain<7>& lowerLayer, const AntiChain<7>& upperLayer) {
	uint64_t bits0 = _mm_extract_epi64(lowerLayer.bf.bitset.data, 0);
	uint64_t bits1 = _mm_extract_epi64(lowerLayer.bf.bitset.data, 1);

	int bits0Size = popcnt64(bits0);
	int bits1Size = popcnt64(bits1);

#ifdef __AVX512F__
	if(bits0Size > bits1Size) {
		if(bits0Size > 3) {
			return getNumChoicesLast2LayersAVX512<false>(bits0, bits1, _mm_extract_epi64(upperLayer.bf.bitset.data, 0), _mm_extract_epi64(upperLayer.bf.bitset.data, 1));
		}
	} else {
		if(bits1Size > 3) {
			return getNumChoicesLast2LayersAVX512<true>(bits1, bits0, _mm_extract_epi64(upperLayer.bf.bitset.data, 1), _mm_extract_epi64(upperLayer.bf.bitset.data, 0));
		}
	}
#endif
#ifdef __AVX2__
	if(bits0Size > bits1Size) {
		if(bits0Size > 2) {
			return getNumChoicesLast2LayersAVX256<false>(bits0, bits1, _mm_extract_epi64(upperLayer.bf.bitset.data, 0), _mm_extract_epi64(upperLayer.bf.bitset.data, 1));
		}
	} else {
		if(bits1Size > 2) {
			return getNumChoicesLast2LayersAVX256<true>(bits1, bits0, _mm_extract_epi64(upperLayer.bf.bitset.data, 1), _mm_extract_epi64(upperLayer.bf.bitset.data, 0));
		}
	}
#endif

	return getNumChoicesForLast2LayersFallback(lowerLayer, upperLayer);
}

template<unsigned int Variables>
uint64_t getNumChoicesRecursiveUp(const BooleanFunction<Variables>& chooseableElements, const Monotonic<Variables>& conflictingMask) {
	using BF = BooleanFunction<Variables>;
	using AC = AntiChain<Variables>;

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
}


