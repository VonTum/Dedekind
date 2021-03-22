#pragma once

#include "connectGraph.h"
#include "allMBFsMap.h"
#include "funcTypes.h"
#include "fileNames.h"
#include "intervalAndSymmetriesMap.h"

template<unsigned int Variables>
void computeDedekindPCoeff() {
	AllMBFMap<Variables, ExtraData> allMBFs = readAllMBFsMapExtraData<Variables>();

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
u192 naivePCoeffMethodMainBot() {
	AllMBFMap<Variables, ExtraData> allIntervalSizes = readAllMBFsMapExtraData<Variables>();

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
