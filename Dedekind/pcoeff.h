#pragma once

#include "connectGraph.h"
#include "allMBFsMap.h"
#include "funcTypes.h"
#include "fileNames.h"
#include "intervalAndSymmetriesMap.h"
#include "parallelIter.h"

template<unsigned int Variables>
uint64_t computePCoefficient(const AntiChain<Variables>& top, const Monotonic<Variables>& bot) {
	size_t connectCount = countConnected(top - bot, bot);
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
u192 basicSymmetriesPCoeffMethod() {
	AllMBFMap<Variables, ExtraData> allIntervalSizes = readAllMBFsMapIntervalSymmetries<Variables>();

	std::atomic<size_t> totalPCoeffs = 0;
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
				uint64_t pcoeff = computePCoefficient(topAC, bot); totalPCoeffs++;
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
u192 noCanonizationPCoeffMethod() {
	AllMBFMap<Variables, ExtraData> allIntervalSizesAndDownLinks = readAllMBFsMapExtraDownLinks<Variables>();

	std::atomic<size_t> totalPCoeffs = 0;
	std::mutex totalMutex;
	u192 total = 0;

	// size to top is D(Variables), one instance, size to bot is 1. pcoeff = 1
	total += allIntervalSizesAndDownLinks.layers.back()[0].value.intervalSizeToBottom;
	totalPCoeffs++;
	
	for(size_t topLayer = 1; topLayer < (1 << Variables) + 1; topLayer++) {
		const BakedMap<Monotonic<Variables>, ExtraData>& curLayer = allIntervalSizesAndDownLinks.layers[topLayer];
		std::cout << "Layer " << topLayer << "  ";
		auto start = std::chrono::high_resolution_clock::now();
		iterCollectionInParallelWithPerThreadBuffer(curLayer, []() {return SwapperLayers<Variables, bool>(); }, [&](const KeyValue<Monotonic<Variables>, ExtraData>& topKV, SwapperLayers<Variables, bool>& touchedEqClasses) {
			u128 subTotal = 0;
			const Monotonic<Variables>& top = topKV.key;

			uint64_t topDuplicity = topKV.value.symmetries;
			uint64_t topIntervalSize = allIntervalSizesAndDownLinks.get(top.dual().canonize()).intervalSizeToBottom;

			//std::cout << "top: " << top << " topIntervalSize: " << topIntervalSize << " topDuplicity: " << topDuplicity << std::endl;
			AntiChain<Variables> topAC = top.asAntiChain();

			// have to hardcode special cases to make iteration below simpler
			subTotal += topKV.value.intervalSizeToBottom; // top == bot -> pcoeff == 1
			totalPCoeffs++;
			//std::cout << top << ": " << 1 << ": ";
			subTotal += 2; // bot == 0 -> intervalSizeToBottom(bot) == 1
			totalPCoeffs++;
			//std::cout << "{}" << ": " << 2 << "; ";



			// skips the class itself as well as class 0
			for(size_t belowLayerI = topLayer-1; belowLayerI > 0; belowLayerI--) {
				const BakedMap<Monotonic<Variables>, ExtraData>& belowLayer = allIntervalSizesAndDownLinks.layers[belowLayerI];

				// simplest first, just iter the whole layer
				for(const KeyValue<Monotonic<Variables>, ExtraData>& botKV : belowLayer) {
					uint64_t botIntervalSize = botKV.value.intervalSizeToBottom;
					
					uint64_t localPCoeffsCount = 0;
					uint64_t pcoeffSum = 0;
					botKV.key.forEachPermutation([&](const Monotonic<Variables>& bot) {
						if(bot <= top) {
							localPCoeffsCount++;
							uint64_t pcoeff = computePCoefficient(topAC, bot);

							//std::cout << bot << ": " << pcoeff << ", ";

							pcoeffSum += pcoeff;
						}
					});
					uint64_t duplication = factorial(Variables) / botKV.value.symmetries;
					assert(localPCoeffsCount % duplication == 0);
					totalPCoeffs += localPCoeffsCount;// / duplication;
					pcoeffSum /= duplication; // remove duplicates

					subTotal += umul128(botIntervalSize, pcoeffSum);
				}
			}

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


