#pragma once

#include "connectGraph.h"
#include "allMBFsMap.h"
#include "funcTypes.h"
#include "fileNames.h"
#include "intervalAndSymmetriesMap.h"
//#include "parallelIter.h"

template<unsigned int Variables>
void computeDedekindPCoeff() {
	AllMBFMap<Variables, ExtraData> allMBFs = readAllMBFsMapIntervalSymmetries<Variables>();

	for(int mainLayerI = 0; mainLayerI < (1 << Variables) + 1; mainLayerI++) {
		const BakedMap<Monotonic<Variables>, ExtraData>& mainLayer = allMBFs.layers[mainLayerI];

		for(const KeyValue<Monotonic<Variables>, ExtraData>& kv : mainLayer) {

		}
	}
}

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

	size_t totalPCoeffs = 0;
	std::mutex totalMutex;
	u192 total = 0;

	for(size_t botLayer = 0; botLayer < (1 << Variables) + 1; botLayer++) {
		const BakedMap<Monotonic<Variables>, ExtraData>& curLayer = allIntervalSizes.layers[botLayer];
		std::cout << "Layer " << botLayer << "  ";
		auto start = std::chrono::high_resolution_clock::now();
		iterCollectionInParallel(curLayer, [&](const KeyValue<Monotonic<Variables>, ExtraData>& kv) {
			u128 subTotal = 0;
			const Monotonic<Variables>& top = kv.key;

			uint64_t topDuplicity = kv.value.symmetries;
			uint64_t topIntervalSize = allIntervalSizes.get(top.dual().canonize()).intervalSizeToBottom;

			//std::cout << "top: " << top << " topIntervalSize: " << topIntervalSize << " topDuplicity: " << topDuplicity << std::endl;
			AntiChain<Variables> topAC = top.asAntiChain();
			forEachMonotonicFunctionUpTo<Variables>(top, [&](const Monotonic<Variables>& bot) {
				uint64_t botIntervalSize = allIntervalSizes.get(bot.canonize()).intervalSizeToBottom;
				uint64_t pcoeff = computePCoefficient(topAC, bot); totalPCoeffs++;
				//std::cout << "  bot: " << bot << " botIntervalSize: " << botIntervalSize << " pcoeff: " << pcoeff << std::endl;
				subTotal += umul128(botIntervalSize, pcoeff);
			});

			std::lock_guard<std::mutex> lg(totalMutex);
			// saves on big multiplications within inner loop
			// topDuplicity * topIntervalSize is allowed since this will be < 64 bits for D(9)
			total += umul192(subTotal, topDuplicity * topIntervalSize);
		});
		auto timeTaken = std::chrono::high_resolution_clock::now() - start;
		std::cout << "time taken: " << (timeTaken.count() / 1000000000.0) << "s, " << getLayerSize<Variables>(botLayer) << " mbfs at " << (timeTaken.count() / 1000.0 / getLayerSize<Variables>(botLayer)) << "us per mbf" << std::endl;
	}

	std::cout << "Used " << totalPCoeffs << " p-coefficients!\n";

	return total;
}

template<unsigned int Variables>
u192 noCanonizationPCoeffMethod() {
	AllMBFMap<Variables, ExtraData> allIntervalSizesAndDownLinks = readAllMBFsMapExtraDownLinks<Variables>();

	size_t totalPCoeffs = 0;
	std::mutex totalMutex;
	u192 total = 0;

	for(size_t botLayer = 0; botLayer < (1 << Variables) + 1; botLayer++) {
		const BakedMap<Monotonic<Variables>, ExtraData>& curLayer = allIntervalSizesAndDownLinks.layers[botLayer];
		std::cout << "Layer " << botLayer << "  ";
		auto start = std::chrono::high_resolution_clock::now();
		iterCollectionInParallelWithPerThreadBuffer(curLayer, []() {return SwapperLayers<Variables, bool>(); }, [&](const KeyValue<Monotonic<Variables>, ExtraData>& kv, SwapperLayers<Variables, bool>& touchedEqClasses) {
			u128 subTotal = 0;
			const Monotonic<Variables>& top = kv.key;

			uint64_t topDuplicity = kv.value.symmetries;
			uint64_t topIntervalSize = allIntervalSizesAndDownLinks.get(top.dual().canonize()).intervalSizeToBottom;

			//std::cout << "top: " << top << " topIntervalSize: " << topIntervalSize << " topDuplicity: " << topDuplicity << std::endl;
			AntiChain<Variables> topAC = top.asAntiChain();

			// TODO change for downLink following
			forEachMonotonicFunctionUpTo<Variables>(top, [&](const Monotonic<Variables>& bot) {
				uint64_t botIntervalSize = allIntervalSizesAndDownLinks.get(bot.canonize()).intervalSizeToBottom;
				uint64_t pcoeff = computePCoefficient(topAC, bot); totalPCoeffs++;
				//std::cout << "  bot: " << bot << " botIntervalSize: " << botIntervalSize << " pcoeff: " << pcoeff << std::endl;
				subTotal += umul128(botIntervalSize, pcoeff);
			});
			// END TODO

			std::lock_guard<std::mutex> lg(totalMutex);
			// saves on big multiplications within inner loop
			// topDuplicity * topIntervalSize is allowed since this will be < 64 bits for D(9)
			total += umul192(subTotal, topDuplicity * topIntervalSize);
		});
		auto timeTaken = std::chrono::high_resolution_clock::now() - start;
		std::cout << "time taken: " << (timeTaken.count() / 1000000000.0) << "s, " << getLayerSize<Variables>(botLayer) << " mbfs at " << (timeTaken.count() / 1000.0 / getLayerSize<Variables>(botLayer)) << "us per mbf" << std::endl;
	}

	std::cout << "Used " << totalPCoeffs << " p-coefficients!\n";

	return total;
}


