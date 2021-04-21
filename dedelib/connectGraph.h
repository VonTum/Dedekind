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
BooleanFunction<Variables> getGroupingMaskNaive(const BooleanFunction<Variables>& graph) {
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
BooleanFunction<Variables> getGroupingMask(const BooleanFunction<Variables>& graph) {
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
		return getGroupingMaskNaive(graph);
	}
}

template<unsigned int Variables>
uint64_t eliminateSingletons(BooleanFunction<Variables>& graph) {
	BooleanFunction<Variables> groupingMask = getGroupingMask(graph);

	BooleanFunction<Variables> singletons = andnot(graph, groupingMask);

	graph = graph & groupingMask; // remove singleton elements, reduces rest of the workload

	return singletons.size();
}

// assumes that no subgraph contains an element which is dominated by an element of another subgraph
template<unsigned int Variables>
uint64_t countConnectedVeryFast(BooleanFunction<Variables> graph, const BooleanFunction<Variables>& initialGuess) {
	assert(!graph.isEmpty());
	assert(!initialGuess.isEmpty());
	assert(initialGuess.isSubSetOf(graph));

	uint64_t totalConnectedComponents = 1;

	BooleanFunction<Variables> expandedDown = initialGuess;

	while(true) {
		do {
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

	return totalConnectedComponents;
}

// assumes that no subgraph contains an element which is dominated by an element of another subgraph
template<unsigned int Variables>
uint64_t countConnectedVeryFast(BooleanFunction<Variables> graph) {
	assert(!graph.isEmpty());
	
	BooleanFunction<Variables> initialGuess = BooleanFunction<Variables>::empty();
	initialGuess.add(graph.getLast());
	initialGuess = initialGuess.monotonizeDown() & graph;

	return countConnectedVeryFast<Variables>(graph, initialGuess);
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


