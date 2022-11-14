#pragma once


#include "smallVector.h"
#include "funcTypes.h"

template<unsigned int Variables>
SmallVector<Monotonic<Variables>, getMaxLayerWidth(Variables)> splitAC(const AntiChain<Variables>& ss) {
	SmallVector<Monotonic<Variables>, getMaxLayerWidth(Variables)> res;
	ss.forEachOne([&res](size_t index) {
		res.push_back(AntiChain<Variables>{index}.asMonotonic());
	});
	return res;
}

/*
	Finds the connected groups within a given list of monotonics where they are linked by links not on d

	Combines the elements of the given list such that
	for two achains a,b in the list, a & b <= d holds
	and for two sets A,B in an achain c in the list
	not ({A & B} <= d) holds.
*/
template<unsigned int Variables>
void connectFast(SmallVector<Monotonic<Variables>, getMaxLayerWidth(Variables)>& res, const Monotonic<Variables>& d) {
	for(size_t i = 0; i < res.size(); i++) {
		doAgain:;
		for(size_t j = i + 1; j < res.size(); j++) {
			if(!((res[i] & res[j]) <= d)) {
				res[i] = res[i] | res[j];
				res[j] = res.back();
				res.pop_back();
				goto doAgain;
			}
		}
	}
}

/*
	Finds the connected groups within a given antichain ss where they are linked by links not on d

	Generate the list of subset achains of the achain ss such that
	for two achains a,b in the list, a & b <= d holds
	and for two sets A,B in an achain c in the list
	not ({A & B} <= d) holds.
*/
template<unsigned int Variables>
SmallVector<Monotonic<Variables>, getMaxLayerWidth(Variables)> connectFast(const AntiChain<Variables>& ss, const Monotonic<Variables>& d) {
	SmallVector<Monotonic<Variables>, getMaxLayerWidth(Variables)> res = splitAC(ss);

	connectFast(res, d);

	return res;
}

/*
	Counts the connected groups within a given list of monotonics where they are linked by links not on d

	Count the resulting size of the list of subsets res
	for two achains a,b in the list, a & b <= d holds
	and for two sets A,B in an achain c in the list
	not ({A & B} <= d) holds.
*/
template<unsigned int Variables>
size_t countConnected(SmallVector<Monotonic<Variables>, getMaxLayerWidth(Variables)>& res, const Monotonic<Variables>& d) {
	connectFast(res, d);
	return res.size();
}

/*
	Counts the connected groups within a given antichain ss where they are linked by links not on d

	Count the size of the list of subset achains of the achain ss such that
	for two achains a,b in the list, a & b <= d holds
	and for two sets A,B in an achain c in the list
	not ({A & B} <= d) holds.
*/
template<unsigned int Variables>
size_t countConnected(const AntiChain<Variables>& ss, const Monotonic<Variables>& d) {
	return connectFast(ss, d).size();
}

template<unsigned int Variables>
bool areAlwaysCombined(const Monotonic<Variables>& a, const Monotonic<Variables>& b, const std::array<int, Variables + 1>& dLayerSizes) {
	Monotonic<Variables> combined = a & b;

	for(int layer = Variables; layer > 0; layer--) {
		int combinedLayerSize = combined.getLayer(layer).size();

		if(combinedLayerSize > dLayerSizes[layer]) { // no possible permutation could cover the whole layer of combined, since the area to cover is larger than the covering area. That means the combination can always be made
			return true;
		}
	}

	return false;
}

template<unsigned int Variables>
std::array<int, Variables+1> getLayerSizes(const Monotonic<Variables>& dClass) {
	std::array<int, Variables + 1> dLayerSizes;
	for(unsigned int i = 0; i < Variables + 1; i++) {
		dLayerSizes[i] = dClass.getLayer(i).size();
	}
	return dLayerSizes;
}

template<unsigned int Variables>
void preCombineConnected(SmallVector<Monotonic<Variables>, getMaxLayerWidth(Variables)>& res, const std::array<int, Variables + 1>& dLayerSizes) {
	for(size_t i = 0; i < res.size(); i++) {
		doAgain:;
		for(size_t j = i + 1; j < res.size(); j++) {
			if(areAlwaysCombined(res[i], res[j], dLayerSizes)) {
				res[i] = res[i] | res[j];
				res[j] = res.back();
				res.pop_back();
				goto doAgain;
			}
		}
	}
}

// returns a mask, that when anded with the original graph yields the elements that have connected elements, and when andnot'ed, yields the singleton elements
template<unsigned int Variables>
BooleanFunction<Variables> exploreGraphNaive(const BooleanFunction<Variables>& graph) {
	BooleanFunction<Variables> totalMaskOut = BooleanFunction<Variables>::empty();
	for(unsigned int v = 0; v < Variables; v++) {
		BooleanFunction<Variables> mask(BooleanFunction<Variables>::varMask(v));

		size_t shift = size_t(1) << v;

		totalMaskOut |= ((graph & mask) >> shift) | (andnot(graph, mask) << shift);
	}

	return totalMaskOut;
}

// returns a mask, that when anded with the original graph yields the elements that have connected elements, and when andnot'ed, yields the singleton elements
template<unsigned int Variables>
BooleanFunction<Variables> exploreGraph(const BooleanFunction<Variables>& graph) {
	if constexpr(Variables == 7) {
		__m128i graphData = graph.bitset.data;
		__m128i totalMaskOut = _mm_shuffle_epi32(graphData, _MM_SHUFFLE(1, 0, 3, 2)); // 6
		totalMaskOut = _mm_or_si128(totalMaskOut, _mm_shuffle_epi32(graphData, _MM_SHUFFLE(2, 3, 0, 1))); // 5
		totalMaskOut = _mm_or_si128(totalMaskOut, _mm_or_si128(_mm_slli_epi32(graphData, 16), _mm_srli_epi32(graphData, 16))); // 4
		totalMaskOut = _mm_or_si128(totalMaskOut, _mm_or_si128(_mm_slli_epi16(graphData, 8), _mm_srli_epi16(graphData, 8))); // 3
		__m128i mask4 = _mm_set1_epi8(0b11110000);
		totalMaskOut = _mm_or_si128(totalMaskOut, _mm_or_si128(_mm_slli_epi16(_mm_andnot_si128(mask4, graphData), 4), _mm_srli_epi16(_mm_and_si128(mask4, graphData), 4))); // 2
		__m128i mask2 = _mm_set1_epi8(0b11001100);
		totalMaskOut = _mm_or_si128(totalMaskOut, _mm_or_si128(_mm_slli_epi16(_mm_andnot_si128(mask2, graphData), 2), _mm_srli_epi16(_mm_and_si128(mask2, graphData), 2))); // 1
		__m128i mask1 = _mm_set1_epi8(0b10101010);
		totalMaskOut = _mm_or_si128(totalMaskOut, _mm_or_si128(_mm_slli_epi16(_mm_andnot_si128(mask1, graphData), 1), _mm_srli_epi16(_mm_and_si128(mask1, graphData), 1))); // 0
		BooleanFunction<Variables> result;
		result.bitset.data = totalMaskOut;
		return result;
	} else {
		return exploreGraphNaive(graph);
	}
}


template<unsigned int Variables>
void eliminateLeavesDown(BooleanFunction<Variables>& graph) {
	// all elements with exactly one connection upward can be removed, they are leaves that do not affect the group count
	BooleanFunction<Variables> shiftedDown0 = (graph.bitset & BooleanFunction<Variables>::varMask(0)) >> 1;
	BooleanFunction<Variables> occuredAtLeastOnce = shiftedDown0;
	BooleanFunction<Variables> blockedElements = BooleanFunction<Variables>::empty();

	for(unsigned int v = 1; v < Variables; v++) {
		BooleanFunction<Variables> shiftedFromAbove((graph.bitset & BooleanFunction<Variables>::varMask(v)) >> (size_t(1) << v));
		blockedElements |= occuredAtLeastOnce & shiftedFromAbove;
		occuredAtLeastOnce |= shiftedFromAbove;
	}

	// need to also block from below
	for(unsigned int v = 0; v < Variables; v++) {
		BooleanFunction<Variables> shiftedFromBelow(andnot(graph.bitset, BooleanFunction<Variables>::varMask(v)) << (size_t(1) << v));
		blockedElements |= shiftedFromBelow;
	}

	BooleanFunction<Variables> elementsToCull = andnot(occuredAtLeastOnce, blockedElements);

	graph = andnot(graph, elementsToCull);
}
template<unsigned int Variables>
void eliminateLeavesUp(BooleanFunction<Variables>& graph) {
	if constexpr(Variables == 7) {
		__m128i data = graph.bitset.data;
		__m128i mask0 = _mm_set1_epi8(0b10101010);
		__m128i shiftedFromBelow0 = _mm_slli_epi16(_mm_andnot_si128(mask0, data), 1);
		__m128i shiftedFromAbove0 = _mm_srli_epi16(_mm_and_si128(mask0, data), 1);
		__m128i occuredAtLeastOnce = shiftedFromBelow0;
		__m128i blockedElements = shiftedFromAbove0;

		{// 1
			__m128i mask = _mm_set1_epi8(0b11001100);
			__m128i shiftedFromBelow = _mm_slli_epi16(_mm_andnot_si128(mask, data), 2);
			__m128i shiftedFromAbove = _mm_srli_epi16(_mm_and_si128(mask, data), 2);
			blockedElements = _mm_or_si128(blockedElements, _mm_or_si128(shiftedFromAbove, _mm_and_si128(occuredAtLeastOnce, shiftedFromBelow)));
			occuredAtLeastOnce = _mm_or_si128(occuredAtLeastOnce, shiftedFromBelow);
		}
		{// 2
			__m128i mask = _mm_set1_epi8(0b11110000);
			__m128i shiftedFromBelow = _mm_slli_epi16(_mm_andnot_si128(mask, data), 4);
			__m128i shiftedFromAbove = _mm_srli_epi16(_mm_and_si128(mask, data), 4);
			blockedElements = _mm_or_si128(blockedElements, _mm_or_si128(shiftedFromAbove, _mm_and_si128(occuredAtLeastOnce, shiftedFromBelow)));
			occuredAtLeastOnce = _mm_or_si128(occuredAtLeastOnce, shiftedFromBelow);
		}
		{// 3
			__m128i shiftedFromBelow = _mm_slli_epi16(data, 8);
			__m128i shiftedFromAbove = _mm_srli_epi16(data, 8);
			blockedElements = _mm_or_si128(blockedElements, _mm_or_si128(shiftedFromAbove, _mm_and_si128(occuredAtLeastOnce, shiftedFromBelow)));
			occuredAtLeastOnce = _mm_or_si128(occuredAtLeastOnce, shiftedFromBelow);
		}
		{// 4
			__m128i shiftedFromBelow = _mm_slli_epi32(data, 16);
			__m128i shiftedFromAbove = _mm_srli_epi32(data, 16);
			blockedElements = _mm_or_si128(blockedElements, _mm_or_si128(shiftedFromAbove, _mm_and_si128(occuredAtLeastOnce, shiftedFromBelow)));
			occuredAtLeastOnce = _mm_or_si128(occuredAtLeastOnce, shiftedFromBelow);
		}
		{// 5
			__m128i shiftedFromBelow = _mm_slli_epi64(data, 32);
			__m128i shiftedFromAbove = _mm_srli_epi64(data, 32);
			blockedElements = _mm_or_si128(blockedElements, _mm_or_si128(shiftedFromAbove, _mm_and_si128(occuredAtLeastOnce, shiftedFromBelow)));
			occuredAtLeastOnce = _mm_or_si128(occuredAtLeastOnce, shiftedFromBelow);
		}
		{// 6
			__m128i shiftedFromBelow = _mm_slli_si128(data, 8);
			__m128i shiftedFromAbove = _mm_srli_si128(data, 8);
			blockedElements = _mm_or_si128(blockedElements, _mm_or_si128(shiftedFromAbove, _mm_and_si128(occuredAtLeastOnce, shiftedFromBelow)));
			occuredAtLeastOnce = _mm_or_si128(occuredAtLeastOnce, shiftedFromBelow);
		}
		__m128i elementsToCull = _mm_andnot_si128(blockedElements, occuredAtLeastOnce);
		graph.bitset.data = _mm_andnot_si128(elementsToCull, data);
		return;
	}
	// all elements with exactly one connection downward can be removed, they are leaves that do not affect the group count
	BooleanFunction<Variables> shiftedFromBelow0 = andnot(graph.bitset, BooleanFunction<Variables>::varMask(0)) << 1;
	BooleanFunction<Variables> shiftedFromAbove0 = (graph.bitset & BooleanFunction<Variables>::varMask(0)) >> 1;
	BooleanFunction<Variables> occuredAtLeastOnce = shiftedFromBelow0;
	BooleanFunction<Variables> blockedElements = shiftedFromAbove0;

	for(unsigned int v = 1; v < Variables; v++) {
		BooleanFunction<Variables> shiftedFromBelow(andnot(graph.bitset, BooleanFunction<Variables>::varMask(v)) << (size_t(1) << v));
		BooleanFunction<Variables> shiftedFromAbove((graph.bitset & BooleanFunction<Variables>::varMask(v)) >> (size_t(1) << v));
		blockedElements |= shiftedFromAbove;
		blockedElements |= occuredAtLeastOnce & shiftedFromBelow;
		occuredAtLeastOnce |= shiftedFromBelow;
	}

	BooleanFunction<Variables> elementsToCull = andnot(occuredAtLeastOnce, blockedElements);

	graph = andnot(graph, elementsToCull);
}


static uint64_t singletonCountHistogram[50];
static uint64_t connectedHistogram[50];


template<unsigned int Variables>
uint64_t eliminateSingletons(BooleanFunction<Variables>& graph) {
	BooleanFunction<Variables> groupingMask = exploreGraph(graph);

	BooleanFunction<Variables> singletons = andnot(graph, groupingMask);
	size_t singletonCount = singletons.size();

	graph = graph & groupingMask; // remove singleton elements, reduces rest of the workload

	singletonCountHistogram[singletonCount]++;

	return singletonCount;
}

//static uint64_t cyclesHistogram[100];

// assumes that no subgraph contains an element which is dominated by an element of another subgraph
template<unsigned int Variables>
uint64_t countConnectedVeryFast(BooleanFunction<Variables> graph, const BooleanFunction<Variables>& initialGuess) {
	assert(!graph.isEmpty());
	assert(!initialGuess.isEmpty());
	assert(initialGuess.isSubSetOf(graph));

	uint64_t totalConnectedComponents = 1;

	BooleanFunction<Variables> expandedDown = initialGuess;

	//int totalCycles = 0;

	while(true) {
		do {
			//totalCycles++;
			BooleanFunction<Variables> expandedUp = expandedDown.monotonizeUp() & graph;
			if(expandedUp == expandedDown) break;
			expandedDown = expandedUp.monotonizeDown() & graph;
			if(expandedUp == expandedDown) break;
		} while(true); // can't just test expandedUp here for some stupid reason, not in scope
		graph = andnot(graph, expandedDown);

		if(graph.isEmpty()) break;

		totalConnectedComponents++;
		expandedDown = BooleanFunction<Variables>::empty();
		expandedDown.add(graph.getLast()); // picks the largest component, first expand downward

		expandedDown = expandedDown.monotonizeDown() & graph; // will always be an expansion, since singletons have been filtered out first
	}

	//cyclesHistogram[totalCycles]++;

	return totalConnectedComponents;
}

template<unsigned int Variables>
uint64_t countConnectedVeryFast(BooleanFunction<Variables> graph) {
	eliminateLeavesUp(graph);
	uint64_t connectCount = eliminateSingletons(graph); // seems to have no effect, or slight pessimization

	if(!graph.isEmpty()) {
		size_t initialGuessIndex = graph.getLast();
		BooleanFunction<Variables> initialGuess = BooleanFunction<Variables>::empty();
		initialGuess.add(initialGuessIndex);
		initialGuess = initialGuess.monotonizeDown() & graph;
		connectCount += countConnectedVeryFast(graph, initialGuess);
	}
	return connectCount;
}

/*template<unsigned int Variables>
uint64_t countConnectedVeryFast(BooleanFunction<Variables> graph) {
	while(!graph.isEmpty()) {
		size_t initialGuessIndex = graph.getLast();
		BooleanFunction<Variables> initialGuess = BooleanFunction<Variables>::empty();
		initialGuess.add(initialGuessIndex);


		do {

		}
	}
}*/

// assumes that no subgraph contains an element which is dominated by an element of another subgraph
template<unsigned int Variables>
uint64_t countConnectedFloodFill(BooleanFunction<Variables> graph) {
	uint64_t totalConnectedComponents = 0;

	//int totalCycles = 0;

	while(!graph.isEmpty()) {
		totalConnectedComponents++;
		BooleanFunction<Variables> expandedDown = BooleanFunction<Variables>::empty();
		expandedDown.add(graph.getLast()); // picks the largest component, first expand downward
		expandedDown = expandedDown.monotonizeDown() & graph; // will always be an expansion, since singletons have been filtered out first

		do {
			//totalCycles++;
			BooleanFunction<Variables> expandedUp = expandedDown.monotonizeUp() & graph;
			if(expandedUp == expandedDown) break;
			expandedDown = expandedUp.monotonizeDown() & graph;
			if(expandedUp == expandedDown) break;
		} while(true); // can't just test expandedUp here for some stupid reason, not in scope
		graph = andnot(graph, expandedDown);
	}

	//cyclesHistogram[totalCycles]++;

	return totalConnectedComponents;
}

// assumes that no subgraph contains an element which is dominated by an element of another subgraph
template<unsigned int Variables>
uint64_t countConnectedSingletonElimination(BooleanFunction<Variables> graph) {
	uint64_t connectCount = eliminateSingletons(graph); // seems to have no effect, or slight pessimization
	connectCount += countConnectedFloodFill(graph);
	++connectedHistogram[connectCount];
	return connectCount;
}



// assumes that no subgraph contains an element which is dominated by an element of another subgraph
template<unsigned int Variables>
uint64_t countConnectedVeryVeryFastBroken(const BooleanFunction<Variables>& graph) {
	BooleanFunction<Variables> totalToEliminate = BooleanFunction<Variables>::empty();
	for(unsigned int shiftUpVar = 0; shiftUpVar < Variables; shiftUpVar++) {
		BooleanFunction<Variables> justUpVar = graph & BooleanFunction<Variables>(~BooleanFunction<Variables>::varMask(shiftUpVar));
		BooleanFunction<Variables> eliminaterMask = (justUpVar << (1 << shiftUpVar)) & graph; // these elements, moving upwards, will eliminate all above them, minus the current var

		for(unsigned int shiftDownVar = shiftUpVar + 1; shiftDownVar < Variables; shiftDownVar++) {
			BooleanFunction<Variables> justDownVar = eliminaterMask & BooleanFunction<Variables>(BooleanFunction<Variables>::varMask(shiftDownVar));
			BooleanFunction<Variables> addedElements = justDownVar >> (1 << shiftDownVar);
			eliminaterMask |= addedElements;
		}

		totalToEliminate |= eliminaterMask;
	}

	BooleanFunction<Variables> groupRepresentatives = graph & ~totalToEliminate;
	return groupRepresentatives.size();
}
