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

static uint64_t preconnectHistogram[50];

// counts how often if(bot <= top)
static uint64_t successfulBots = 0;
static uint64_t failedBots = 0;

static void printHistogramAndPCoeffs(unsigned int Variables) {
	std::cout << "Used " << totalPCoeffs << " p-coefficients!\n";
	//totalPCoeffs = 0;

	std::cout << "Connections: \n";
	for(size_t i = 0; i <= getMaxLayerWidth(Variables); i++) {
		std::cout << i << ": " << connectedHistogram[i] << "\n";
		//connectedHistogram[i] = 0;
	}


	std::cout << "Preconnections: \n";
	for(size_t i = 0; i <= getMaxLayerWidth(Variables); i++) {
		std::cout << i << ": " << preconnectHistogram[i] << "\n";
		//preconnectHistogram[i] = 0;
	}

	std::cout << "Singleton counts: \n";
	for(size_t i = 0; i <= getMaxLayerWidth(Variables); i++) {
		std::cout << i << ": " << singletonCountHistogram[i] << "\n";
		//singletonCountHistogram[i] = 0;
	}

	std::cout << "Cycles taken: \n";
	for(size_t i = 0; i < 100; i++) {
		std::cout << i << ": " << cyclesHistogram[i] << "\n";
		//cyclesHistogram[i] = 0;
	}

	std::cout << "Bottoms: " << successfulBots << "/" << (successfulBots + failedBots) << " bots were (bot <= top) " << (100.0 * successfulBots / (successfulBots + failedBots)) << "%" << std::endl;
	//successfulBots = 0;
	//failedBots = 0;
}

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
uint64_t computePCoefficientFast(const BooleanFunction<Variables>& graph, const BooleanFunction<Variables>& initialGuess) {
	size_t connectCount = countConnectedVeryFast(graph, initialGuess);
	++totalPCoeffs;
	++connectedHistogram[connectCount];
	uint64_t pcoeff = uint64_t(1) << connectCount;
	return pcoeff;
}
template<unsigned int Variables>
uint64_t computePCoefficientFast(const BooleanFunction<Variables>& graph) {
	size_t connectCount = countConnectedVeryFast(graph);
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
			successfulBots++;
		} else {
			failedBots++;
		}
	});
	return totalPCoeff;
}

template<size_t Size>
bool atLeastOneIsLarger(const std::array<int, Size>& a, const std::array<int, Size>& b) {
	for(size_t i = 0; i < Size; i++) {
		if(a[i] > b[i]) {
			return true;
		}
	}
	return false;
}

// expects a function of the form uint64_t(BooleanFunction<Variables> graph)
template<unsigned int Variables, typename Func>
uint64_t sumPermutedPCoeffsOver(const Monotonic<Variables>& top, const Monotonic<Variables>& botClass, const Func& computePCoeff) {
	uint64_t totalPCoeff = 0;
	botClass.forEachPermutation([&](const Monotonic<Variables>& permutedBot) {
		if(permutedBot <= top) {
			BooleanFunction<Variables> graph = andnot(top.bf, permutedBot.bf);
			totalPCoeff += computePCoeff(graph);
			successfulBots++;
		} else {
			failedBots++;
		}
	});
	return totalPCoeff;
}

template<unsigned int Variables>
uint64_t computePermutationPCoeffSumFast(SmallVector<Monotonic<Variables>, getMaxLayerWidth(Variables)> splitTop, const Monotonic<Variables>& top, const Monotonic<Variables>& botClass) {
	assert(splitTop.size() >= 1);

	std::array<int, Variables + 1> botLayerSizes = getLayerSizes(botClass);

	preCombineConnected(splitTop, botLayerSizes);
	preconnectHistogram[splitTop.size()]++;
	Monotonic<Variables> biggestGuess = splitTop[0];
	size_t biggestGuessSize = biggestGuess.size();
	for(size_t i = 1; i < splitTop.size(); i++) {
		size_t guessSize = splitTop[i].size();
		if(guessSize > biggestGuessSize) {
			biggestGuessSize = guessSize;
			biggestGuess = splitTop[i];
		}
	}

	/*switch(splitTop.size()) {
	case 1:
		return sumPermutedPCoeffsOver(top, botClass, [&](const BooleanFunction<Variables>& graph) {
			++totalPCoeffs;
			++connectedHistogram[1];
			return 2;
		});
	case 2:
	{
		Monotonic<Variables> overlap01 = splitTop[0] & splitTop[1];
		return sumPermutedPCoeffsOver(top, botClass, [&](const BooleanFunction<Variables>& graph) {
			if((graph & overlap01.bf).isEmpty()) {
				return 2; // 2 groups (no overlap)
			} else {
				return 4; // 1 group
			}
		});
	}
	case 3:
	{
		Monotonic<Variables> overlap01 = splitTop[0] & splitTop[1];
		Monotonic<Variables> overlap02 = splitTop[0] & splitTop[2];
		Monotonic<Variables> overlap12 = splitTop[1] & splitTop[2];
		
		return sumPermutedPCoeffsOver(top, botClass, [&](const BooleanFunction<Variables>& graph) {
			int numOverlaps = 0;
			if(!(graph & overlap01.bf).isEmpty()) numOverlaps++;
			if(!(graph & overlap02.bf).isEmpty()) numOverlaps++;
			if(!(graph & overlap12.bf).isEmpty()) numOverlaps++;

			int overlapsResults[4]{8,4,2,2};
			return overlapsResults[numOverlaps];
		});
	}
	}*/
	std::array<int, Variables + 1> biggestGuessSizes = getLayerSizes(biggestGuess);

	if(atLeastOneIsLarger(biggestGuessSizes, botLayerSizes) && biggestGuess.asAntiChain().size() >= 2) { // there is one layer which will always be larger than bot, so it's guaranteed to be included
		return sumPermutedPCoeffsOver(top, botClass, [&](BooleanFunction<Variables> graph) {
			uint64_t connectCount = eliminateSingletons(graph); // seems to have no effect, or slight pessimization

			BooleanFunction<Variables> initialGuess = biggestGuess.bf & graph;
			assert(!initialGuess.isEmpty());
			connectCount += countConnectedVeryFast(graph, initialGuess);
			++totalPCoeffs;
			++connectedHistogram[connectCount];
			return uint64_t(1) << connectCount;
		});
	}

	return sumPermutedPCoeffsOver(top, botClass, [&](BooleanFunction<Variables> graph) {
		uint64_t connectCount = eliminateSingletons(graph); // seems to have no effect, or slight pessimization

		if(!graph.isEmpty()) {
			size_t initialGuessIndex = graph.getLast();
			BooleanFunction<Variables> initialGuess = BooleanFunction<Variables>::empty();
			initialGuess.add(initialGuessIndex);
			initialGuess = initialGuess.monotonizeDown() & graph;
			connectCount += countConnectedVeryFast(graph, initialGuess);
		}
		++totalPCoeffs;
		++connectedHistogram[connectCount];
		return uint64_t(1) << connectCount;
	});
}

template<unsigned int Variables>
u192 basicSymmetriesPCoeffMethod() {
	AllMBFMap<Variables, ExtraData> allIntervalSizes = readAllMBFsMapIntervalSymmetries<Variables>();

	std::mutex totalMutex;
	u192 total = 0;

	for(size_t topLayerI = 0; topLayerI < (1 << Variables) + 1; topLayerI++) {
		const BakedMap<Monotonic<Variables>, ExtraData>& topLayer = allIntervalSizes.layers[topLayerI];
		std::cout << "Layer " << topLayerI << "  ";
		auto start = std::chrono::high_resolution_clock::now();
		iterCollectionInParallel(topLayer, [&](const KeyValue<Monotonic<Variables>, ExtraData>& topKV) {
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
		std::cout << "time taken: " << (timeTaken.count() / 1000000000.0) << "s, " << getLayerSize<Variables>(topLayerI) << " mbfs at " << (timeTaken.count() / 1000.0 / getLayerSize<Variables>(topLayerI)) << "us per mbf" << std::endl;
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
				successfulBots++;
				uint64_t pcoeff = computePCoefficient(splitTops[i], bot);

				//std::cout << bot << ": " << pcoeff << ", ";

				pcoeffSums[i] += pcoeff;
			} else {
				failedBots++;
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
	
	for(int topLayerI = 1; topLayerI < (1 << Variables) + 1; topLayerI++) {
		const BakedMap<Monotonic<Variables>, ExtraData>& topLayer = allIntervalSizesAndDownLinks.layers[topLayerI];
		std::cout << "Layer " << topLayerI << "  ";
		auto start = std::chrono::high_resolution_clock::now();
		iterCollectionInParallelWithPerThreadBuffer(iterInBlocks<TOPS_PER_BLOCK>(topLayer), []() {return SwapperLayers<Variables, BitSet<TOPS_PER_BLOCK>>(BitSet<TOPS_PER_BLOCK>::empty()); }, 
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

			int belowLayerI = topLayerI - 1;
			// skips the class itself as well as class 0
			for(; belowLayerI > std::max(0, topLayerI - 10); belowLayerI--) {
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
		std::cout << "time taken: " << (timeTaken.count() / 1000000000.0) << "s, " << getLayerSize<Variables>(topLayerI) << " mbfs at " << (timeTaken.count() / 1000.0 / getLayerSize<Variables>(topLayerI)) << "us per mbf" << std::endl;
	}

	printHistogramAndPCoeffs(Variables);
	std::cout << "D(" << (Variables + 2) << ") = " << total << std::endl;

	return total;
}

// expects a function of the form void(KeyValue<Monotonic<Variables>, ExtraData> botKV)
template<unsigned int Variables, typename Func>
void forEachBelowUsingSwapper(size_t topLayerI, size_t topIndex, const AllMBFMap<Variables, ExtraData>& allIntervalSizesAndDownLinks, SwapperLayers<Variables, bool>& swapper, const Func& funcToRun) {
	const BakedMap<Monotonic<Variables>, ExtraData>& topLayer = allIntervalSizesAndDownLinks.layers[topLayerI];

	swapper.clearSource();
	swapper.clearDestination();

	DownConnection* initialDownConnectionsStart = topLayer[topIndex].value.downConnections;
	DownConnection* initialDownConnectionsEnd = topLayer[topIndex + 1].value.downConnections;

	for(DownConnection* cur = initialDownConnectionsStart; cur != initialDownConnectionsEnd; ++cur) {
		assert(cur->id < getLayerSize<Variables>(topLayerI - 1));
		swapper.set(cur->id);
	}

	swapper.pushNext();

	for(int belowLayerI = topLayerI - 1; belowLayerI > 0; belowLayerI--) {
		const BakedMap<Monotonic<Variables>, ExtraData>& belowLayer = allIntervalSizesAndDownLinks.layers[belowLayerI];

		swapper.forEachSourceElement([&](size_t index, bool wasContained) {
			assert(wasContained);
			assert(index < getLayerSize<Variables>(belowLayerI));

			const KeyValue<Monotonic<Variables>, ExtraData>& botKV = belowLayer[index];

			DownConnection* downConnectionsStart = botKV.value.downConnections;
			DownConnection* downConnectionsEnd = belowLayer[index + 1].value.downConnections;

			for(DownConnection* cur = downConnectionsStart; cur != downConnectionsEnd; ++cur) {
				assert(cur->id < getLayerSize<Variables>(belowLayerI - 1));
				swapper.set(cur->id);
			}

			funcToRun(botKV);
		});
		swapper.pushNext();
	}
}

template<unsigned int Variables>
u192 computePCoeffSum(size_t topLayerI, size_t topIndex, const AllMBFMap<Variables, ExtraData>& allIntervalSizesAndDownLinks, SwapperLayers<Variables, bool>& swapper) {
	const BakedMap<Monotonic<Variables>, ExtraData>& topLayer = allIntervalSizesAndDownLinks.layers[topLayerI];
	const KeyValue<Monotonic<Variables>, ExtraData>& topKV = topLayer[topIndex];

	uint64_t withItself = topKV.value.intervalSizeToBottom;
	uint64_t withBot = 2;
	
	u128 subTotal(withItself + withBot);
	
	AntiChain<Variables> topAC = topKV.key.asAntiChain();
	SmallVector<Monotonic<Variables>, getMaxLayerWidth(Variables)> splitTopAC = splitAC(topAC);

	forEachBelowUsingSwapper(topLayerI, topIndex, allIntervalSizesAndDownLinks, swapper, [&](const KeyValue<Monotonic<Variables>, ExtraData>& botKV) {
		uint64_t totalPCoeff = computePermutationPCoeffSumFast(splitTopAC, topKV.key, botKV.key);
		uint64_t duplication = factorial(Variables) / botKV.value.symmetries;
		subTotal += umul128(totalPCoeff / duplication, botKV.value.intervalSizeToBottom);
	});

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
	
	for(int topLayerI = 1; topLayerI < (1 << Variables) + 1; topLayerI++) {
		const BakedMap<Monotonic<Variables>, ExtraData>& topLayer = allIntervalSizesAndDownLinks.layers[topLayerI];
		std::cout << "Layer " << topLayerI << "  ";
		auto start = std::chrono::high_resolution_clock::now();
		std::mutex totalMutex;
		total += iterCollectionPartitionedWithSeparateTotalsWithBuffers(IntRange<size_t>{size_t(0), topLayer.size()}, u192(0), [&]() {return SwapperLayers<Variables, bool>(); }, [&](size_t topIdx, u192& subTotal, SwapperLayers<Variables, bool>& swapper) {
			subTotal += computePCoeffSum(topLayerI, topIdx, allIntervalSizesAndDownLinks, swapper);
		}, [](u192& a, u192 b) {a += b; });
		auto timeTaken = std::chrono::high_resolution_clock::now() - start;
		std::cout << "time taken: " << (timeTaken.count() / 1000000000.0) << "s, " << getLayerSize<Variables>(topLayerI) << " mbfs at " << (timeTaken.count() / 1000.0 / getLayerSize<Variables>(topLayerI)) << "us per mbf" << std::endl;
	}

	printHistogramAndPCoeffs(Variables);
	std::cout << "D(" << (Variables + 2) << ") = " << total << std::endl;

	return total;
}

#include <random>

template<unsigned int Variables>
void pcoeffTimeEstimate() {
	AllMBFMap<Variables, ExtraData> allIntervalSizesAndDownLinks = readAllMBFsMapExtraDownLinks<Variables>();

	// size to top is D(Variables), one instance, size to bot is 1. pcoeff = 1
	//total += allIntervalSizesAndDownLinks.layers.back()[0].value.intervalSizeToBottom;

	std::default_random_engine generator;
	std::mutex mtx;

	double totalSeconds = 0.0;

	iterCollectionInParallel(IntRange<int>{1, 1 << Variables}, [&](size_t topLayerI) {
		const BakedMap<Monotonic<Variables>, ExtraData>& topLayer = allIntervalSizesAndDownLinks.layers[topLayerI];
		
		auto start = std::chrono::high_resolution_clock::now();

		mtx.lock();
		std::uniform_int_distribution<size_t> distribution(0, getLayerSize<Variables>(topLayerI));
		size_t selectedIndex = distribution(generator);
		mtx.unlock();

		SwapperLayers<Variables, bool> swapper;
		u192 subTotal = computePCoeffSum(topLayerI, selectedIndex, allIntervalSizesAndDownLinks, swapper);
		
		mtx.lock();
		std::cout << "Layer " << topLayerI << "  " << subTotal << "\n";

		auto timeTaken = std::chrono::high_resolution_clock::now() - start;
		double secondsTaken = timeTaken.count() / 1000000000.0;
		double estTotalSeconds = secondsTaken * getLayerSize<Variables>(topLayerI);
		std::cout << "time taken: " << secondsTaken << "s for 1 mbf, " << getLayerSize<Variables>(topLayerI) << " mbfs totalling " << estTotalSeconds << " core seconds in total" << std::endl;
		totalSeconds += estTotalSeconds;
		mtx.unlock();
	});

	printHistogramAndPCoeffs(Variables);
	std::cout << "Estimated total seconds: " << totalSeconds << std::endl;
}

template<unsigned int Variables>
void pcoeffLayerElementStats(size_t topLayerI) {
	AllMBFMap<Variables, ExtraData> allIntervalSizesAndDownLinks = readAllMBFsMapExtraDownLinks<Variables>();

	// size to top is D(Variables), one instance, size to bot is 1. pcoeff = 1
	//total += allIntervalSizesAndDownLinks.layers.back()[0].value.intervalSizeToBottom;

	std::default_random_engine generator;

	double totalSeconds = 0.0;

	const BakedMap<Monotonic<Variables>, ExtraData>& topLayer = allIntervalSizesAndDownLinks.layers[topLayerI];

	auto start = std::chrono::high_resolution_clock::now();

	std::uniform_int_distribution<size_t> distribution(0, getLayerSize<Variables>(topLayerI));
	size_t selectedIndex = distribution(generator);

	SwapperLayers<Variables, bool> swapper;
	u192 subTotal = computePCoeffSum(topLayerI, selectedIndex, allIntervalSizesAndDownLinks, swapper);

	std::cout << "Layer " << topLayerI << "  " << subTotal << "\n";

	auto timeTaken = std::chrono::high_resolution_clock::now() - start;
	double secondsTaken = timeTaken.count() / 1000000000.0;
	double estTotalSeconds = secondsTaken * getLayerSize<Variables>(topLayerI);
	std::cout << "time taken: " << secondsTaken << "s for 1 mbf, " << getLayerSize<Variables>(topLayerI) << " mbfs totalling " << estTotalSeconds << " core seconds in total" << std::endl;
	totalSeconds += estTotalSeconds;

	printHistogramAndPCoeffs(Variables);
	std::cout << "Estimated total seconds: " << totalSeconds << std::endl;
}