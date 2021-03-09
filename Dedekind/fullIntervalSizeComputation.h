#pragma once

#include "bufferedMap.h"
#include "booleanFunction.h"
#include "funcTypes.h"
#include "allMBFsMap.h"
#include "intervalSizeFromBottom.h"
#include "parallelIter.h"
#include "fileNames.h"
#include <fstream>



template<unsigned int Variables>
void computeIntervals() {
	using MBF = Monotonic<Variables>;
	using LR = Layer<Variables>;

	std::ifstream mbfFile(allMBFSSorted<Variables>(), std::ios::binary);
	if(!mbfFile.is_open()) throw "Error not found!";

	AllMBFMap<Variables, uint64_t> allMBFs = AllMBFMap<Variables, uint64_t>::readKeysFile(mbfFile);

	assert(allMBFs.layers[0][0].key.isEmpty());
	allMBFs.layers[0][0].value = 1;
	assert(allMBFs.layers[1][0].key == AntiChain<Variables>{0}.asMonotonic());
	allMBFs.layers[1][0].value = 2;
	assert(allMBFs.layers[2][0].key == AntiChain<Variables>{1}.asMonotonic());
	allMBFs.layers[2][0].value = 3;

	for(int layer = 3; layer < (1 << Variables) + 1; layer++) {
		std::cout << "Layer " << layer << ":\n";
		for(KeyValue<MBF, uint64_t>& cur : allMBFs.layers[layer]) {
			//std::cout << "  " << cur.key;

			LR topLayer = cur.key.getTopLayer();

			size_t removedElement = topLayer.getFirst();

			MBF smallerMBF = cur.key;
			smallerMBF.remove(removedElement);

			BakedMap<MBF, uint64_t>& prevLayer = allMBFs.layers[layer - 1];
			//for(auto& i : prevLayer) std::cout << i.key << " ";

			uint64_t smallerMBFIntervalSize = prevLayer.get(smallerMBF.canonize());

			uint64_t thisIntervalSize = computeIntervalSizeExtention(smallerMBF, smallerMBFIntervalSize, removedElement);
			cur.value = thisIntervalSize;

			//std::cout << ": " << thisIntervalSize << "\n";
		}
	}

	if(allMBFs.layers.back()[0].value != dedekindNumbers[Variables]) {
		throw "Not correct!";
	}

	std::ofstream intervalsFile(allIntervals<Variables>(), std::ios::binary);
	if(!intervalsFile.is_open()) throw "Error not found!";

	allMBFs.writeToFile(intervalsFile, [](uint64_t intervalSize, std::ofstream& of) {
		serializeU64(intervalSize, of);
	});
}
template<unsigned int Variables>
void computeIntervalsParallel() {
	using MBF = Monotonic<Variables>;
	using LR = Layer<Variables>;

	std::ifstream mbfFile(allMBFSSorted<Variables>(), std::ios::binary);
	if(!mbfFile.is_open()) throw "Error not found!";

	AllMBFMap<Variables, uint64_t> allMBFs = AllMBFMap<Variables, uint64_t>::readKeysFile(mbfFile);

	assert(allMBFs.layers[0][0].key.isEmpty());
	allMBFs.layers[0][0].value = 1;
	assert(allMBFs.layers[1][0].key == AntiChain<Variables>{0}.asMonotonic());
	allMBFs.layers[1][0].value = 2;
	assert(allMBFs.layers[2][0].key == AntiChain<Variables>{1}.asMonotonic());
	allMBFs.layers[2][0].value = 3;

	for(int layer = 3; layer < (1 << Variables) + 1; layer++) {
		std::cout << "Layer " << layer << ":\n";
		iterCollectionInParallel(allMBFs.layers[layer], [&](KeyValue<MBF, uint64_t>& cur) {
			//std::cout << "  " << cur.key;

			LR topLayer = cur.key.getTopLayer();

			size_t removedElement = topLayer.getFirst();

			MBF smallerMBF = cur.key;
			smallerMBF.remove(removedElement);

			BakedMap<MBF, uint64_t>& prevLayer = allMBFs.layers[layer - 1];
			//for(auto& i : prevLayer) std::cout << i.key << " ";

			uint64_t smallerMBFIntervalSize = prevLayer.get(smallerMBF.canonize());

			uint64_t thisIntervalSize = computeIntervalSizeExtention(smallerMBF, smallerMBFIntervalSize, removedElement);
			cur.value = thisIntervalSize;
		});
	}

	if(allMBFs.layers.back()[0].value != dedekindNumbers[Variables]) {
		throw "Not correct!";
	}

	std::ofstream intervalsFile(allIntervals<Variables>(), std::ios::binary);
	if(!intervalsFile.is_open()) throw "Error not found!";

	allMBFs.writeToFile(intervalsFile, [](uint64_t intervalSize, std::ofstream& of) {
		serializeU64(intervalSize, of);
	});
}

