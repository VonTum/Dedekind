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
uint64_t computeExtendedIntervalOf(const BakedMap<Monotonic<Variables>, uint64_t>& prevLayer, const Monotonic<Variables>& cur) {
	//std::cout << "  " << cur;

	Layer<Variables> topLayer = cur.getTopLayer();

	size_t removedElement = topLayer.getFirst();

	Monotonic<Variables> smallerMBF = cur;
	smallerMBF.remove(removedElement);

	//for(auto& i : prevLayer) std::cout << i.key << " ";

	uint64_t smallerMBFIntervalSize = prevLayer.get(smallerMBF.canonize());

	uint64_t thisIntervalSize = computeIntervalSizeExtention(smallerMBF, smallerMBFIntervalSize, removedElement);
	assert(thisIntervalSize == intervalSizeFast(Monotonic<Variables>::getBot(), cur));

	//std::cout << ": " << thisIntervalSize << "\n";
	return thisIntervalSize;
}


template<unsigned int Variables>
void computeIntervals() {
	using MBF = Monotonic<Variables>;
	using LR = Layer<Variables>;

	std::ifstream mbfFile(FileName::allMBFSSorted(Variables), std::ios::binary);
	if(!mbfFile.is_open()) throw "Error not found!";

	AllMBFMap<Variables, uint64_t> allMBFs = AllMBFMap<Variables, uint64_t>::readKeysFile(mbfFile);

	assert(allMBFs.layers[0][0].key.isEmpty());
	allMBFs.layers[0][0].value = 1;
	/*assert(allMBFs.layers[1][0].key == AntiChain<Variables>{0}.asMonotonic());
	allMBFs.layers[1][0].value = 2;
	assert(allMBFs.layers[2][0].key == AntiChain<Variables>{1}.asMonotonic());
	allMBFs.layers[2][0].value = 3;*/

	{
		std::ofstream intervalsFile(FileName::allIntervals(Variables), std::ios::binary);
		if(!intervalsFile.is_open()) throw "Error not found!";

		writeLayerToFile(intervalsFile, allMBFs.layers[0], 0, [](uint64_t intervalSize, std::ofstream& of) {
			serializeU64(intervalSize, of);
		});

		intervalsFile.close();
	}

	for(int layer = 1; layer < (1 << Variables) + 1; layer++) {
		std::cout << "Layer " << layer << ": " << std::flush;
		BakedMap<MBF, uint64_t>& prevLayer = allMBFs.layers[layer - 1];
		auto start = std::chrono::high_resolution_clock::now();
		for(KeyValue<MBF, uint64_t>& cur : allMBFs.layers[layer]) {
			cur.value = computeExtendedIntervalOf(prevLayer, cur.key);
		}
		auto timeTaken = std::chrono::high_resolution_clock::now() - start;
		std::cout << "time taken: " << (timeTaken.count() / 1000000000.0) << "s, " << getLayerSize<Variables>(layer) << " mbfs at " << (timeTaken.count() / 1000.0 / getLayerSize<Variables>(layer)) << "us per mbf" << std::endl;

		{
			std::ofstream intervalsFile(FileName::allIntervals(Variables), std::ios::binary | std::ios::app);
			if(!intervalsFile.is_open()) throw "Error not found!";

			writeLayerToFile(intervalsFile, allMBFs.layers[layer], layer, [](uint64_t intervalSize, std::ofstream& of) {
				serializeU64(intervalSize, of);
			});

			intervalsFile.close();
		}
	}
	/*
	std::ofstream intervalsFile(FileName::allIntervals<Variables>(), std::ios::binary);
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

	std::ifstream mbfFile(FileName::allMBFSSorted(Variables), std::ios::binary);
	if(!mbfFile.is_open()) throw "Error not found!";

	AllMBFMap<Variables, uint64_t> allMBFs = AllMBFMap<Variables, uint64_t>::readKeysFile(mbfFile);

	assert(allMBFs.layers[0][0].key.isEmpty());
	allMBFs.layers[0][0].value = 1;
	/*assert(allMBFs.layers[1][0].key == AntiChain<Variables>{0}.asMonotonic());
	allMBFs.layers[1][0].value = 2;
	assert(allMBFs.layers[2][0].key == AntiChain<Variables>{1}.asMonotonic());
	allMBFs.layers[2][0].value = 3;*/

	{
		std::ofstream intervalsFile(FileName::allIntervals(Variables), std::ios::binary);
		if(!intervalsFile.is_open()) throw "Error not found!";

		writeLayerToFile(intervalsFile, allMBFs.layers[0], 0, [](uint64_t intervalSize, std::ofstream& of) {
			serializeU64(intervalSize, of);
		});

		intervalsFile.close();
	}

	for(int layer = 1; layer < (1 << Variables) + 1; layer++) {
		std::cout << "Layer " << layer << ": " << std::flush;
		BakedMap<MBF, uint64_t>& prevLayer = allMBFs.layers[layer - 1];
		auto start = std::chrono::high_resolution_clock::now();
		iterCollectionInParallel(allMBFs.layers[layer], [&](KeyValue<MBF, uint64_t>& cur) {
			cur.value = computeExtendedIntervalOf(prevLayer, cur.key);
		});
		auto timeTaken = std::chrono::high_resolution_clock::now() - start;
		std::cout << "time taken: " << (timeTaken.count() / 1000000000.0) << "s, " << getLayerSize<Variables>(layer) << " mbfs at " << (timeTaken.count() / 1000.0 / getLayerSize<Variables>(layer)) << "us per mbf" << std::endl;

		{
			std::ofstream intervalsFile(FileName::allIntervals(Variables), std::ios::binary | std::ios::app);
			if(!intervalsFile.is_open()) throw "Error not found!";

			writeLayerToFile(intervalsFile, allMBFs.layers[layer], layer, [](uint64_t intervalSize, std::ofstream& of) {
				serializeU64(intervalSize, of);
			});

			intervalsFile.close();
		}
	}
	/*
	std::ofstream intervalsFile(FileName::allIntervals(Variables), std::ios::binary);
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
	std::ifstream intervalsFile(FileName::allIntervals(Variables), std::ios::binary);
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


template<unsigned int Variables>
std::array<BakedMap<Monotonic<Variables>, uint64_t>, 2> getTwoIntervalLayers(size_t bottomLayer) {
	std::ifstream intervalFile(FileName::allIntervals(Variables), std::ios::binary);
	if(!intervalFile.is_open()) {
		std::cout << FileName::allIntervals(Variables);
		throw "File not found! ";
	}

	skipLayersInFile<Variables>(intervalFile, 0, bottomLayer, sizeof(uint64_t));

	auto layer1 = readLayerFromFile<Variables, uint64_t>(intervalFile, bottomLayer, [](std::ifstream& is) {return deserializeU64(is); });
	auto layer2 = readLayerFromFile<Variables, uint64_t>(intervalFile, bottomLayer + 1, [](std::ifstream& is) {return deserializeU64(is); });

	intervalFile.close();

	return {std::move(layer1), std::move(layer2)};
}

template<unsigned int Variables>
void checkIntervalLayers(size_t bottomLayer) {
	using MBF = Monotonic<Variables>;
	using LR = Layer<Variables>;

	std::array<BakedMap<Monotonic<Variables>, uint64_t>, 2> layers = getTwoIntervalLayers<Variables>(bottomLayer);

	BakedMap<Monotonic<Variables>, uint64_t>& layer1 = layers[0];
	BakedMap<Monotonic<Variables>, uint64_t>& layer2 = layers[1];

	auto totalStart = std::chrono::high_resolution_clock::now();
	//int count = 0;
	for(KeyValue<Monotonic<Variables>, uint64_t>& cur : layer2) {
		LR topLayer = cur.key.getTopLayer();

		size_t removedElement = topLayer.getFirst();

		MBF smallerMBF = cur.key;
		smallerMBF.remove(removedElement);

		uint64_t smallerMBFIntervalSize = layer1.get(smallerMBF.canonize());

		auto start = std::chrono::high_resolution_clock::now();
		uint64_t thisIntervalSize = computeIntervalSizeExtention(smallerMBF, smallerMBFIntervalSize, removedElement);
		if(cur.value != thisIntervalSize) {
			std::cout << "\nIncorrect size: cur.value: " << cur.value << "   thisIntervalSize: " << thisIntervalSize << std::endl;
			throw "END";
		}

		{
			auto deltaMillis = (std::chrono::high_resolution_clock::now() - start).count() / 1000000.0;
			//if(deltaMillis > 36.0) {
			//	std::cout << "\n" << cur.key << "+" << FunctionInput{uint32_t(removedElement)} << ": " << deltaMillis << "ms\n";
				//throw "END";
			//} else {
				std::cout << '.' << deltaMillis << "ms ";
			//}
		}

		// for benchmarking
		/*count++;
		if(count >= 10000) {
			auto deltaMills = (std::chrono::high_resolution_clock::now() - totalStart).count() / 1000000.0;

			std::cout << "Time: " << deltaMills << "  time per: " << deltaMills / count << "\n";
			throw "END";
		}*/
	}
}

