#pragma once

#include "bufferedMap.h"
#include "booleanFunction.h"
#include "funcTypes.h"
#include "allMBFsMap.h"
#include "intervalSizeFromBottom.h"
#include "parallelIter.h"
#include "fileNames.h"

#include <fstream>
#include <chrono>


template<unsigned int Variables>
void computeIntervals() {
	using MBF = Monotonic<Variables>;
	using LR = Layer<Variables>;

	std::ifstream mbfFile(allMBFSSorted(Variables), std::ios::binary);
	if(!mbfFile.is_open()) throw "Error not found!";

	AllMBFMap<Variables, uint64_t> allMBFs = AllMBFMap<Variables, uint64_t>::readKeysFile(mbfFile);

	assert(allMBFs.layers[0][0].key.isEmpty());
	allMBFs.layers[0][0].value = 1;
	/*assert(allMBFs.layers[1][0].key == AntiChain<Variables>{0}.asMonotonic());
	allMBFs.layers[1][0].value = 2;
	assert(allMBFs.layers[2][0].key == AntiChain<Variables>{1}.asMonotonic());
	allMBFs.layers[2][0].value = 3;*/

	{
		std::ofstream intervalsFile(allIntervals(Variables), std::ios::binary);
		if(!intervalsFile.is_open()) throw "Error not found!";

		writeLayerToFile(intervalsFile, allMBFs.layers[0], 0, [](uint64_t intervalSize, std::ofstream& of) {
			serializeU64(intervalSize, of);
		});

		intervalsFile.close();
	}

	for(int layer = 1; layer < (1 << Variables) + 1; layer++) {
		std::cout << "Layer " << layer << ": " << std::flush;
		auto start = std::chrono::high_resolution_clock::now();
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
		auto timeTaken = std::chrono::high_resolution_clock::now() - start;
		std::cout << "time taken: " << (timeTaken.count() / 1000000000.0) << "s, " << getLayerSize<Variables>(layer) << " mbfs at " << (timeTaken.count() / 1000.0 / getLayerSize<Variables>(layer)) << "us per mbf" << std::endl;

		{
			std::ofstream intervalsFile(allIntervals(Variables), std::ios::binary | std::ios::app);
			if(!intervalsFile.is_open()) throw "Error not found!";

			writeLayerToFile(intervalsFile, allMBFs.layers[layer], layer, [](uint64_t intervalSize, std::ofstream& of) {
				serializeU64(intervalSize, of);
			});

			intervalsFile.close();
		}
	}
	/*
	std::ofstream intervalsFile(allIntervals<Variables>(), std::ios::binary);
	if(!intervalsFile.is_open()) throw "Error not found!";

	allMBFs.writeToFile(intervalsFile, [](uint64_t intervalSize, std::ofstream& of) {
		serializeU64(intervalSize, of);
	});
	*/
	if(allMBFs.layers.back()[0].value != dedekindNumbers[Variables]) {
		std::cout << "Not correct!" << std::endl;
	}
}
template<unsigned int Variables>
void computeIntervalsParallel() {
	using MBF = Monotonic<Variables>;
	using LR = Layer<Variables>;

	std::ifstream mbfFile(allMBFSSorted(Variables), std::ios::binary);
	if(!mbfFile.is_open()) throw "Error not found!";

	AllMBFMap<Variables, uint64_t> allMBFs = AllMBFMap<Variables, uint64_t>::readKeysFile(mbfFile);

	assert(allMBFs.layers[0][0].key.isEmpty());
	allMBFs.layers[0][0].value = 1;
	/*assert(allMBFs.layers[1][0].key == AntiChain<Variables>{0}.asMonotonic());
	allMBFs.layers[1][0].value = 2;
	assert(allMBFs.layers[2][0].key == AntiChain<Variables>{1}.asMonotonic());
	allMBFs.layers[2][0].value = 3;*/

	{
		std::ofstream intervalsFile(allIntervals(Variables), std::ios::binary);
		if(!intervalsFile.is_open()) throw "Error not found!";

		writeLayerToFile(intervalsFile, allMBFs.layers[0], 0, [](uint64_t intervalSize, std::ofstream& of) {
			serializeU64(intervalSize, of);
		});

		intervalsFile.close();
	}

	for(int layer = 1; layer < (1 << Variables) + 1; layer++) {
		std::cout << "Layer " << layer << ": " << std::flush;
		auto start = std::chrono::high_resolution_clock::now();
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
		auto timeTaken = std::chrono::high_resolution_clock::now() - start;
		std::cout << "time taken: " << (timeTaken.count() / 1000000000.0) << "s, " << getLayerSize<Variables>(layer) << " mbfs at " << (timeTaken.count() / 1000.0 / getLayerSize<Variables>(layer)) << "us per mbf" << std::endl;

		{
			std::ofstream intervalsFile(allIntervals(Variables), std::ios::binary | std::ios::app);
			if(!intervalsFile.is_open()) throw "Error not found!";

			writeLayerToFile(intervalsFile, allMBFs.layers[layer], layer, [](uint64_t intervalSize, std::ofstream& of) {
				serializeU64(intervalSize, of);
			});

			intervalsFile.close();
		}
	}
	/*
	std::ofstream intervalsFile(allIntervals(Variables), std::ios::binary);
	if(!intervalsFile.is_open()) throw "Error not found!";

	allMBFs.writeToFile(intervalsFile, [](uint64_t intervalSize, std::ofstream& of) {
		serializeU64(intervalSize, of);
	});
	*/
	if(allMBFs.layers.back()[0].value != dedekindNumbers[Variables]) {
		std::cout << "Not correct!" << std::endl;
	}
}



template<unsigned int Variables>
void verifyIntervalsCorrect() {
	std::ifstream intervalsFile(allIntervals(Variables), std::ios::binary);
	if(!intervalsFile.is_open()) throw "Error not found!";

	for(size_t layer = 0; layer < (1 << Variables) + 1; layer++) {
		auto map = readLayerFromFile<Variables, uint64_t>(intervalsFile, layer, [](std::ifstream& is) {return deserializeU64(is); });
		for(const KeyValue<Monotonic<Variables>, uint64_t>& item : map) {
			if(item.key.size() != layer) {
				throw "Bad map key! Key size is not layer size";
			}
			uint64_t realSize = intervalSizeFast(Monotonic<Variables>::getBot(), item.key);
			if(item.value != realSize) {
				throw "Bad interval size!";
			}
		}
		std::cout << ".";
	}

	intervalsFile.close();
}

