#pragma once

#include "allMBFsMap.h"
#include "funcTypes.h"
#include "parallelIter.h"

struct ExtraData {
	uint64_t symmetries : 16;
	uint64_t intervalSizeToBottom : 48;
};

void serializeExtraData(const ExtraData& item, std::ofstream& os) {
	serializeU16(static_cast<uint16_t>(item.symmetries), os);
	serializeU48(item.intervalSizeToBottom, os);
}

ExtraData deserializeExtraData(std::ifstream& is) {
	ExtraData result;
	result.symmetries = deserializeU16(is);
	result.intervalSizeToBottom = deserializeU48(is);
	return result;
}

template<unsigned int Variables>
void addSymmetriesToIntervalFile() {
	std::ifstream intervals(FileName::allIntervals(Variables), std::ios::binary);
	std::ofstream intervalSymmetries(FileName::allIntervalSymmetries(Variables), std::ios::binary);

	u128 totalWork = 0;
	u128 dedekindNumber = 0;
	for(size_t layer = 0; layer < (1 << Variables) + 1; layer++) {
		std::cout << "Layer " << layer << ": ";
		auto start = std::chrono::high_resolution_clock::now();

		KeyValue<Monotonic<Variables>, ExtraData>* foundLayer = readBufFromFile<Variables, ExtraData>(intervals, layer, [](std::ifstream& is) {ExtraData result; result.intervalSizeToBottom = deserializeU64(is); return result; });

		size_t size = getLayerSize<Variables>(layer);
		finishIterInParallel(foundLayer, foundLayer + size, [&](KeyValue<Monotonic<Variables>, ExtraData>& kv) {
			kv.value.symmetries = kv.key.bf.countNonDuplicatePermutations();
		});

		writeBufToFile<Variables, ExtraData>(intervalSymmetries, foundLayer, layer, serializeExtraData);

		double deltaMicros = (std::chrono::high_resolution_clock::now() - start).count() / 1000.0;
		std::cout << " Time taken: " << deltaMicros / 1000000.0 << "s for " << size << " mbfs at " << deltaMicros / size << "us per mbf" << std::endl;

		delete[] foundLayer;
	}
}

template<unsigned int Variables>
AllMBFMap<Variables, ExtraData> readAllMBFsMapExtraData() {
	std::ifstream symFile(FileName::allIntervalSymmetries(Variables), std::ios::binary);
	if(!symFile.is_open()) throw "File not opened!";
	return AllMBFMap<Variables, ExtraData>::readMapFile(symFile, deserializeExtraData);
}


template<unsigned int Variables>
void computeDPlus1() {
	std::ifstream intervals(FileName::allIntervals(Variables), std::ios::binary);

	u128 totalWork = 0;
	u128 dedekindNumber = 0;
	for(size_t layer = 0; layer < (1 << Variables) + 1; layer++) {
		std::cout << "Layer " << layer << ": ";
		auto start = std::chrono::high_resolution_clock::now();

		KeyValue<Monotonic<Variables>, uint64_t>* foundLayer = readBufFromFile<Variables, uint64_t>(intervals, layer, [](std::ifstream& is) {return deserializeU64(is); });
		size_t size = getLayerSize<Variables>(layer);

		for(size_t i = 0; i < size; i++) {
			totalWork += u128(foundLayer[i].value);
		}

		u128 fullTotalForThisLayer = finishIterPartitionedWithSeparateTotals(foundLayer, foundLayer + size, u128(0), [&](const KeyValue<Monotonic<Variables>, uint64_t>& kv, u128& localTotal) {
			localTotal += umul128(kv.value, kv.key.bf.countNonDuplicatePermutations());
		}, [](u128& a, const u128& b) {a += b; });

		dedekindNumber += fullTotalForThisLayer;

		double deltaMicros = (std::chrono::high_resolution_clock::now() - start).count() / 1000.0;
		std::cout << " Time taken: " << deltaMicros / 1000000.0 << "s for " << size << " mbfs at " << deltaMicros / size << "us per mbf" << std::endl;
	}

	std::cout << "Amount of work: " << totalWork << std::endl;
	std::cout << "D(" << Variables + 1 << "): " << dedekindNumber << std::endl;
}





