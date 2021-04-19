#pragma once

#include "u192.h"
#include "connectGraph.h"
#include "allMBFsMap.h"
#include "funcTypes.h"
#include "fileNames.h"
#include "intervalAndSymmetriesMap.h"
#include "parallelIter.h"
#include "blockIterator.h"
#include <atomic>

static uint64_t connectedHistogram[50];

constexpr size_t TOPS_PER_BLOCK = 4;
constexpr double swapperCutoff = 0.5;
static size_t totalPCoeffs(0);

template<unsigned int Variables>
uint64_t computePCoefficient(const AntiChain<Variables>& top, const Monotonic<Variables>& bot) {
	size_t connectCount = countConnected(top - bot, bot);
	++totalPCoeffs;
	++connectedHistogram[connectCount];
	uint64_t pcoeff = uint64_t(1) << connectCount;
	return pcoeff;
}

template<unsigned int Variables>
uint64_t computePCoefficient(const SmallVector<Monotonic<Variables>, getMaxLayerWidth(Variables)>& top, const Monotonic<Variables>& bot) {
	SmallVector<Monotonic<Variables>, getMaxLayerWidth(Variables)> resultingTop;
	for(const Monotonic<Variables>& mbf : top) {
		if(!(mbf <= bot)) {
			resultingTop.push_back(mbf);
		}
	}
	size_t connectCount = countConnected(resultingTop, bot);
	++totalPCoeffs;
	++connectedHistogram[connectCount];
	uint64_t pcoeff = uint64_t(1) << connectCount;
	return pcoeff;
}

template<unsigned int Variables>
uint64_t computePCoefficientFast(const SmallVector<Monotonic<Variables>, getMaxLayerWidth(Variables)>& splitTop, const BooleanFunction<Variables>& graph) {
	size_t connectCount = countConnectedVeryFast(splitTop, graph);
	++totalPCoeffs;
	++connectedHistogram[connectCount];
	uint64_t pcoeff = uint64_t(1) << connectCount;
	return pcoeff;
}

template<unsigned int Variables>
u192 naivePCoeffMethod() {
	AllMBFMap<Variables, ExtraData> allIntervalSizes = readAllMBFsMapIntervalSymmetries<Variables>();

	u192 total = 0;
	forEachMonotonicFunction<Variables>([&](const Monotonic<Variables>& top) {
		uint64_t topIntervalSize = allIntervalSizes.get(top.dual().canonize()).intervalSizeToBottom;
		//std::cout << "top: " << top << " topIntervalSize: " << topIntervalSize << std::endl;
		AntiChain<Variables> topAC = top.asAntiChain();
		forEachMonotonicFunctionUpTo<Variables>(top, [&](const Monotonic<Variables>& bot) {
			uint64_t botIntervalSize = allIntervalSizes.get(bot.canonize()).intervalSizeToBottom;
			uint64_t pcoeff = computePCoefficient(topAC, bot);
			//std::cout << "  bot: " << bot << " botIntervalSize: " << botIntervalSize << " pcoeff: " << pcoeff << std::endl;
			total += umul192(umul128(botIntervalSize, pcoeff), topIntervalSize);
		});
	});
	return total;
}

template<unsigned int Variables>
uint64_t computePermutationPCoeffSum(const SmallVector<Monotonic<Variables>, getMaxLayerWidth(Variables)>& splitTop, const Monotonic<Variables>& top, const Monotonic<Variables>& botClass) {
	uint64_t totalPCoeff = 0;
	botClass.forEachPermutation([&](const Monotonic<Variables>& permutedBot) {
		if(permutedBot <= top) {
			totalPCoeff += computePCoefficient(splitTop, permutedBot);
		}
	});
	return totalPCoeff;
}

template<unsigned int Variables>
uint64_t computePermutationPCoeffSumFast(const SmallVector<Monotonic<Variables>, getMaxLayerWidth(Variables)>& splitTop, const Monotonic<Variables>& top, const Monotonic<Variables>& botClass) {
	uint64_t totalPCoeff = 0;
	botClass.forEachPermutation([&](const Monotonic<Variables>& permutedBot) {
		if(permutedBot <= top) {
			totalPCoeff += computePCoefficientFast(splitTop, andnot(top.bf, permutedBot.bf));
		}
	});
	return totalPCoeff;
}

static void printHistogramAndPCoeffs(unsigned int Variables) {
	std::cout << "Used " << totalPCoeffs << " p-coefficients!\n";

	std::cout << "Connections: \n";
	for(size_t i = 0; i <= getMaxLayerWidth(Variables); i++) {
		std::cout << i << ": " << connectedHistogram[i] << "\n";
		connectedHistogram[i] = 0;
	}

	totalPCoeffs = 0;

}

template<unsigned int Variables>
u192 basicSymmetriesPCoeffMethod() {
	AllMBFMap<Variables, ExtraData> allIntervalSizes = readAllMBFsMapIntervalSymmetries<Variables>();

	std::mutex totalMutex;
	u192 total = 0;

	for(size_t topLayer = 0; topLayer < (1 << Variables) + 1; topLayer++) {
		const BakedMap<Monotonic<Variables>, ExtraData>& curLayer = allIntervalSizes.layers[topLayer];
		std::cout << "Layer " << topLayer << "  ";
		auto start = std::chrono::high_resolution_clock::now();
		iterCollectionInParallel(curLayer, [&](const KeyValue<Monotonic<Variables>, ExtraData>& topKV) {
			u128 subTotal = 0;
			const Monotonic<Variables>& top = topKV.key;

			uint64_t topDuplicity = topKV.value.symmetries;
			uint64_t topIntervalSize = allIntervalSizes.get(top.dual().canonize()).intervalSizeToBottom;

			//std::cout << "top: " << top << " topIntervalSize: " << topIntervalSize << " topDuplicity: " << topDuplicity << std::endl;
			AntiChain<Variables> topAC = top.asAntiChain();
			forEachMonotonicFunctionUpTo<Variables>(top, [&](const Monotonic<Variables>& bot) {
				uint64_t botIntervalSize = allIntervalSizes.get(bot.canonize()).intervalSizeToBottom;
				uint64_t pcoeff = computePCoefficient(topAC, bot);
				//std::cout << "  bot: " << bot << " botIntervalSize: " << botIntervalSize << " pcoeff: " << pcoeff << std::endl;

				//std::cout << bot << ": " << pcoeff << ", ";

				subTotal += umul128(botIntervalSize, pcoeff);
			});

			std::lock_guard<std::mutex> lg(totalMutex);
			// saves on big multiplications within inner loop
			// topDuplicity * topIntervalSize is allowed since this will be < 64 bits for D(9)
			total += umul192(subTotal, topDuplicity * topIntervalSize);
		});

		//std::cout << total;
		auto timeTaken = std::chrono::high_resolution_clock::now() - start;
		std::cout << "time taken: " << (timeTaken.count() / 1000000000.0) << "s, " << getLayerSize<Variables>(topLayer) << " mbfs at " << (timeTaken.count() / 1000.0 / getLayerSize<Variables>(topLayer)) << "us per mbf" << std::endl;
	}

	std::cout << "Used " << totalPCoeffs << " p-coefficients!\n";

	return total;
}

template<unsigned int Variables>
std::array<u128, TOPS_PER_BLOCK> getBotToSubTotals(
	const std::array<Monotonic<Variables>, TOPS_PER_BLOCK>& topMBFs, 
	const std::array<SmallVector<Monotonic<Variables>, getMaxLayerWidth(Variables)>, TOPS_PER_BLOCK>& splitTops,
	const KeyValue<Monotonic<Variables>, ExtraData>& botKV, 
	size_t size) {

	uint64_t botIntervalSize = botKV.value.intervalSizeToBottom;

	std::array<uint64_t, TOPS_PER_BLOCK> pcoeffSums; for(uint64_t& item : pcoeffSums) { item = 0; }
	botKV.key.forEachPermutation([&](const Monotonic<Variables>& bot) {
		for(size_t i = 0; i < size; i++) {
			if(bot <= topMBFs[i]) {
				uint64_t pcoeff = computePCoefficient(splitTops[i], bot);

				//std::cout << bot << ": " << pcoeff << ", ";

				pcoeffSums[i] += pcoeff;
			}
		}
	});
	uint64_t duplication = factorial(Variables) / botKV.value.symmetries;

	std::array<u128, TOPS_PER_BLOCK> result;
	for(size_t i = 0; i < size; i++) {
		result[i] = umul128(botIntervalSize, pcoeffSums[i] / duplication); // divide to remove duplicates
	}
	return result;
}

template<unsigned int Variables>
u192 noCanonizationPCoeffMethod() {
	AllMBFMap<Variables, ExtraData> allIntervalSizesAndDownLinks = readAllMBFsMapExtraDownLinks<Variables>();

	std::mutex totalMutex;
	u192 total(0);

	u128 intervalSize0 = allIntervalSizesAndDownLinks.layers.back()[0].value.intervalSizeToBottom;

	// size to top is D(Variables), one instance, size to bot is 1. pcoeff = 1
	total += intervalSize0;
	
	for(int topLayer = 1; topLayer < (1 << Variables) + 1; topLayer++) {
		const BakedMap<Monotonic<Variables>, ExtraData>& curLayer = allIntervalSizesAndDownLinks.layers[topLayer];
		std::cout << "Layer " << topLayer << "  ";
		auto start = std::chrono::high_resolution_clock::now();
		iterCollectionInParallelWithPerThreadBuffer(iterInBlocks<TOPS_PER_BLOCK>(curLayer), []() {return SwapperLayers<Variables, BitSet<TOPS_PER_BLOCK>>(BitSet<TOPS_PER_BLOCK>::empty()); }, 
													[&](const SmallVector<const KeyValue<Monotonic<Variables>, ExtraData>*, TOPS_PER_BLOCK>& topKVs, SwapperLayers<Variables, BitSet<TOPS_PER_BLOCK>>& touchedEqClasses) {
			touchedEqClasses.clearSource();
			touchedEqClasses.clearDestination();

			std::array<u128, TOPS_PER_BLOCK> subTotals;
			std::array<SmallVector<Monotonic<Variables>, getMaxLayerWidth(Variables)>, TOPS_PER_BLOCK> splitTops;
			for(size_t i = 0; i < topKVs.size(); i++) {
				subTotals[i] = 0;
				AntiChain<Variables> topAC = topKVs[i]->key.asAntiChain();
				splitTops[i] = splitAC(topAC);

				// have to hardcode special cases to make iteration below simpler
				subTotals[i] += topKVs[i]->value.intervalSizeToBottom; // top == bot -> pcoeff == 1
				subTotals[i] += 2; // bot == {} -> intervalSizeToBottom(bot) == 1

				DownConnection* from = topKVs[i]->value.downConnections;
				DownConnection* to = (topKVs[i]+1)->value.downConnections;
				for(; from != to; from++) {
					touchedEqClasses.dest(from->id).set(i);
				}
			}
			touchedEqClasses.pushNext();

			int belowLayerI = topLayer - 1;
			// skips the class itself as well as class 0
			for(; belowLayerI > std::max(0, topLayer - 10); belowLayerI--) {
				const BakedMap<Monotonic<Variables>, ExtraData>& belowLayer = allIntervalSizesAndDownLinks.layers[belowLayerI];

				// simplest first, just iter the whole layer
				for(size_t index : touchedEqClasses) {
					assert(index < getLayerSize<Variables>(belowLayerI));
					BitSet<TOPS_PER_BLOCK> touchedPerMBF = touchedEqClasses[index];

					const KeyValue<Monotonic<Variables>, ExtraData>& botKV = belowLayer[index];

					std::array<size_t, TOPS_PER_BLOCK> indices;
					std::array<SmallVector<Monotonic<Variables>, getMaxLayerWidth(Variables)>, TOPS_PER_BLOCK> newSplitTops;
					std::array<Monotonic<Variables>, TOPS_PER_BLOCK> topMBFs;
					size_t size = 0;
					for(size_t i = 0; i < topKVs.size(); i++) {
						if(touchedPerMBF.get(i)) {
							indices[size] = i;
							newSplitTops[size] = splitTops[i];
							topMBFs[size] = topKVs[i]->key;
							size++;
						}
					}

					std::array<u128, TOPS_PER_BLOCK> extraSubTotals = getBotToSubTotals(topMBFs, newSplitTops, botKV, size);
					for(size_t i = 0; i < size; i++) {
						subTotals[indices[i]] += extraSubTotals[i];
					}

					DownConnection* from = botKV.value.downConnections;
					DownConnection* to = (&botKV + 1)->value.downConnections;
					
					for(; from != to; from++) {
						assert(from->id < getLayerSize<Variables>(belowLayerI - 1));
						touchedEqClasses.dest(from->id) |= touchedEqClasses[index];
					}
				}

				touchedEqClasses.pushNext();
				if(touchedEqClasses.dirtyDestinationList.size() >= swapperCutoff * getLayerSize<Variables>(belowLayerI)) {
					belowLayerI--;
					break;
				}
			}

			std::array<Monotonic<Variables>, TOPS_PER_BLOCK> topMBFs;
			for(size_t i = 0; i < topKVs.size(); i++) {
				topMBFs[i] = topKVs[i]->key;
			}

			// skips the class itself as well as class 0
			for(; belowLayerI > 0; belowLayerI--) {
				const BakedMap<Monotonic<Variables>, ExtraData>& belowLayer = allIntervalSizesAndDownLinks.layers[belowLayerI];

				// simplest first, just iter the whole layer
				for(const KeyValue<Monotonic<Variables>, ExtraData>& botKV : belowLayer) {
					std::array<SmallVector<Monotonic<Variables>, getMaxLayerWidth(Variables)>, TOPS_PER_BLOCK> modifiedSplitTops = splitTops;
					std::array<int, Variables + 1> dLayerSizes = getLayerSizes(botKV.key);
					for(size_t i = 0; i < topKVs.size(); i++) {
						preCombineConnected(modifiedSplitTops[i], dLayerSizes);
					}

					std::array<u128, TOPS_PER_BLOCK> extraSubTotals = getBotToSubTotals(topMBFs, modifiedSplitTops, botKV, topKVs.size());

					for(size_t i = 0; i < topKVs.size(); i++) {
						subTotals[i] += extraSubTotals[i];
					}
				}
			}


			// saves on big multiplications within inner loop
			// topDuplicity * topIntervalSize is allowed since this will be < 64 bits for D(9)

			u192 totalToAddToTotal = 0;

			for(size_t i = 0; i < topKVs.size(); i++) {
				uint64_t topDuplicity = topKVs[i]->value.symmetries;
				uint64_t topIntervalSize = allIntervalSizesAndDownLinks.get(topKVs[i]->key.dual().canonize()).intervalSizeToBottom;
				u192 addToTotal = umul192(subTotals[i], topDuplicity * topIntervalSize);
				totalToAddToTotal += addToTotal;
			}
			std::lock_guard<std::mutex> lg(totalMutex);
			total += totalToAddToTotal;
		});
		auto timeTaken = std::chrono::high_resolution_clock::now() - start;
		std::cout << "time taken: " << (timeTaken.count() / 1000000000.0) << "s, " << getLayerSize<Variables>(topLayer) << " mbfs at " << (timeTaken.count() / 1000.0 / getLayerSize<Variables>(topLayer)) << "us per mbf" << std::endl;
	}

	printHistogramAndPCoeffs(Variables);
	std::cout << "D(" << (Variables + 2) << ") = " << total << std::endl;

	return total;
}

template<unsigned int Variables>
u192 computePCoeffSum(size_t topLayer, const KeyValue<Monotonic<Variables>, ExtraData>& topKV, const AllMBFMap<Variables, ExtraData>& allIntervalSizesAndDownLinks) {
	uint64_t withItself = topKV.value.intervalSizeToBottom;
	uint64_t withBot = 2;
	
	u128 subTotal(withItself + withBot);
	
	AntiChain<Variables> topAC = topKV.key.asAntiChain();
	SmallVector<Monotonic<Variables>, getMaxLayerWidth(Variables)> splitTopAC = splitAC(topAC);

	for(int belowLayerI = topLayer - 1; belowLayerI > 0; belowLayerI--) {
		const BakedMap<Monotonic<Variables>, ExtraData>& belowLayer = allIntervalSizesAndDownLinks.layers[belowLayerI];

		// simplest first, just iter the whole layer
		for(const KeyValue<Monotonic<Variables>, ExtraData>& botKV : belowLayer) {
			uint64_t totalPCoeff = computePermutationPCoeffSum(splitTopAC, topKV.key, botKV.key);
			uint64_t duplication = factorial(Variables) / botKV.value.symmetries;
			subTotal += umul128(totalPCoeff / duplication, botKV.value.intervalSizeToBottom);
		}
	}

	const ExtraData& topKVDual = allIntervalSizesAndDownLinks.get(topKV.key.dual().canonize());
	return umul192(subTotal, topKV.value.symmetries * topKVDual.intervalSizeToBottom);
}

template<unsigned int Variables>
u192 pcoeffMethodV2() {
	AllMBFMap<Variables, ExtraData> allIntervalSizesAndDownLinks = readAllMBFsMapExtraDownLinks<Variables>();

	std::mutex totalMutex;
	u192 total(0);

	// size to top is D(Variables), one instance, size to bot is 1. pcoeff = 1
	total += allIntervalSizesAndDownLinks.layers.back()[0].value.intervalSizeToBottom;
	
	for(int topLayer = 1; topLayer < (1 << Variables) + 1; topLayer++) {
		const BakedMap<Monotonic<Variables>, ExtraData>& curLayer = allIntervalSizesAndDownLinks.layers[topLayer];
		std::cout << "Layer " << topLayer << "  ";
		auto start = std::chrono::high_resolution_clock::now();
		std::mutex totalMutex;
		total += iterCollectionPartitionedWithSeparateTotals(curLayer, u192(0), [&](const KeyValue<Monotonic<Variables>, ExtraData>& topKV, u192& subTotal) {
			subTotal += computePCoeffSum(topLayer, topKV, allIntervalSizesAndDownLinks);
		}, [](u192& a, u192 b) {a += b; });
		auto timeTaken = std::chrono::high_resolution_clock::now() - start;
		std::cout << "time taken: " << (timeTaken.count() / 1000000000.0) << "s, " << getLayerSize<Variables>(topLayer) << " mbfs at " << (timeTaken.count() / 1000.0 / getLayerSize<Variables>(topLayer)) << "us per mbf" << std::endl;
	}

	printHistogramAndPCoeffs(Variables);
	std::cout << "D(" << (Variables + 2) << ") = " << total << std::endl;

	return total;
}
