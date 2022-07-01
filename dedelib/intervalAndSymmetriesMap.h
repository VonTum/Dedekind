#pragma once

#include "allMBFsMap.h"
#include "funcTypes.h"
#include "parallelIter.h"
#include "u192.h"

struct DownConnection {
	uint32_t id;
};

struct IntervalSymmetry {
	uint64_t symmetries : 16;
	uint64_t intervalSizeToBottom : 48;
};

struct ExtraData : public IntervalSymmetry {
	ExtraData() = default;
	ExtraData(const IntervalSymmetry& is) : IntervalSymmetry(is) {}
	ExtraData& operator=(const IntervalSymmetry& is) { symmetries = is.symmetries; intervalSizeToBottom = is.intervalSizeToBottom; return *this; }
	DownConnection* downConnections;
};

inline void serializeExtraData(const IntervalSymmetry& item, std::ofstream& os) {
	serializeU16(static_cast<uint16_t>(item.symmetries), os);
	serializeU48(item.intervalSizeToBottom, os);
}

inline IntervalSymmetry deserializeExtraData(std::ifstream& is) {
	IntervalSymmetry result;
	result.symmetries = deserializeU16(is);
	result.intervalSizeToBottom = deserializeU48(is);
	return result;
}

template<unsigned int Variables>
void addDownConnections(BakedMap<Monotonic<Variables>, ExtraData>& upper, const BakedMap<Monotonic<Variables>, ExtraData>& lower, DownConnection* connectionbuf) {
	for(KeyValue<Monotonic<Variables>, ExtraData>& kv : upper) {
		AntiChain<Variables> removeableElements = kv.key.asAntiChain();

		SmallVector<Monotonic<Variables>, getMaxLayerWidth(Variables)> foundBelowElements;

		removeableElements.forEachOne([&](size_t elementIndex) {
			Monotonic<Variables> elementBelow = kv.key;
			elementBelow.remove(elementIndex);

			elementBelow = elementBelow.canonize();

			if(!foundBelowElements.contains(elementBelow)) {
				foundBelowElements.push_back(elementBelow);
			}
		});

		kv.value.downConnections = connectionbuf;
		for(const Monotonic<Variables>& item : foundBelowElements) {
			connectionbuf->id = static_cast<uint32_t>(lower.indexOf(item));
			connectionbuf++;
		}
		(&kv+1)->value.downConnections = connectionbuf;
	}
}

template<unsigned int Variables>
void addSymmetriesToIntervalFile() {
	std::ifstream intervals(FileName::allIntervals(Variables), std::ios::binary);
	std::ofstream intervalSymmetries(FileName::allIntervalSymmetries(Variables), std::ios::binary);

	for(size_t layer = 0; layer < (1 << Variables) + 1; layer++) {
		std::cout << "Layer " << layer << ": ";
		auto start = std::chrono::high_resolution_clock::now();

		KeyValue<Monotonic<Variables>, ExtraData>* foundLayer = readBufFromFile<Variables, ExtraData>(intervals, layer, [](std::ifstream& is) {ExtraData result; result.intervalSizeToBottom = deserializeU64(is); return result; });

		size_t size = layerSizes[Variables][layer];
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
AllMBFMap<Variables, ExtraData> readAllMBFsMapIntervalSymmetries() {
	std::ifstream symFile(FileName::allIntervalSymmetries(Variables), std::ios::binary);
	if(!symFile.is_open()) throw "File not opened!";
	return AllMBFMap<Variables, ExtraData>::readMapFile(symFile, deserializeExtraData);
}

template<unsigned int Variables>
AllMBFMap<Variables, ExtraData> readAllMBFsMapExtraDownLinks() {
	AllMBFMap<Variables, ExtraData> map = readAllMBFsMapIntervalSymmetries<Variables>();
	iterCollectionInParallel(IntRange<size_t>{1, map.layers.size()}, [&](size_t layerIndex) {
		BakedMap<Monotonic<Variables>, ExtraData>& upperLayer = map.layers[layerIndex];
		const BakedMap<Monotonic<Variables>, ExtraData>& lowerLayer = map.layers[layerIndex-1];

		size_t linkCountInLayer = linkCounts[Variables][layerIndex-1];
		DownConnection* downLinksBuf = new DownConnection[linkCountInLayer];

		addDownConnections(upperLayer, lowerLayer, downLinksBuf);
	});
	return map;
}


template<unsigned int Variables>
void computeDPlus1() {
	std::ifstream intervalSymmetries(FileName::allIntervalSymmetries(Variables), std::ios::binary);

	u128 totalWork = 0;
	u128 dedekindNumber = 0;
	for(size_t layer = 0; layer < (1 << Variables) + 1; layer++) {
		std::cout << "Layer " << layer << ": ";
		auto start = std::chrono::high_resolution_clock::now();

		KeyValue<Monotonic<Variables>, IntervalSymmetry>* foundLayer = readBufFromFile<Variables, IntervalSymmetry>(intervalSymmetries, layer, deserializeExtraData);
		size_t size = layerSizes[Variables][layer];

		for(size_t i = 0; i < size; i++) {
			totalWork += u128(foundLayer[i].value.intervalSizeToBottom);
		}

		u128 fullTotalForThisLayer = finishIterPartitionedWithSeparateTotals(foundLayer, foundLayer + size, u128(0), [&](const KeyValue<Monotonic<Variables>, IntervalSymmetry>& kv, u128& localTotal) {
			localTotal += umul128(kv.value.intervalSizeToBottom, kv.value.symmetries);
		}, [](u128& a, const u128& b) {a += b; });

		dedekindNumber += fullTotalForThisLayer;

		double deltaMicros = (std::chrono::high_resolution_clock::now() - start).count() / 1000.0;
		std::cout << " Time taken: " << deltaMicros / 1000000.0 << "s for " << size << " mbfs at " << deltaMicros / size << "us per mbf" << std::endl;

		delete[] foundLayer;
	}

	std::cout << "Amount of work: " << totalWork << std::endl;
	std::cout << "D(" << Variables + 1 << "): " << dedekindNumber << std::endl;
}





