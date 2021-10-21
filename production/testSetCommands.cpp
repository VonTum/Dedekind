#include "commands.h"

#include "../dedelib/fileNames.h"
#include "../dedelib/allMBFsMap.h"
#include "../dedelib/intervalAndSymmetriesMap.h"
#include "../dedelib/toString.h"
#include "../dedelib/serialization.h"
#include "../dedelib/pcoeff.h"

#include "../dedelib/flatMBFStructure.h"

#include <random>

template<unsigned int Variables>
void benchmarkSample() {
	std::ifstream symFile(FileName::allIntervalSymmetries(Variables), std::ios::binary);
	if(!symFile.is_open()) throw "File not opened!";

	std::vector<std::pair<Monotonic<Variables>, IntervalSymmetry>> benchSet;
	benchSet.reserve(mbfCounts[Variables] / 60);

	std::default_random_engine generator;

	while(symFile.good()) {
		Monotonic<Variables> mbf = deserializeMBF<Variables>(symFile);
		IntervalSymmetry is = deserializeExtraData(symFile);

		if(rand() % 100 == 0) {
			// keep
			permuteRandom(mbf, generator);

			benchSet.push_back(std::make_pair(mbf, is));
		}
	}
	symFile.close();

	std::shuffle(benchSet.begin(), benchSet.end(), generator);

	std::ofstream benchmarkSetFile(FileName::benchmarkSet(Variables), std::ios::binary);
	if(!benchmarkSetFile.is_open()) throw "File not opened!";

	for(const auto& item : benchSet) {
		serializeMBF(item.first, benchmarkSetFile);
		serializeExtraData(item.second, benchmarkSetFile);
	}

	benchmarkSetFile.close();
}

template<unsigned int Variables>
void benchmarkSampleTopBots(int topFraction = 1, int botFraction = 1) {
	AllMBFMap<Variables, ExtraData> allIntervalSizesAndDownLinks = readAllMBFsMapExtraDownLinks<Variables>();

	std::cout << "Read map" << std::endl;

	std::vector<TopBots<Variables>> benchSet;
	std::mutex benchSetMutex;
	
	std::atomic<int> randSeed(std::chrono::steady_clock::now().time_since_epoch().count());

	for(int topLayerI = 1; topLayerI < (1 << Variables) + 1; topLayerI++) {
		std::cout << "Layer " << topLayerI << std::endl;
		const BakedMap<Monotonic<Variables>, ExtraData>& topLayer = allIntervalSizesAndDownLinks.layers[topLayerI];

		iterCollectionInParallelWithPerThreadBuffer(IntRange<size_t>{size_t(0), topLayer.size()}, 
													[&]() {return std::make_pair(SwapperLayers<Variables, bool>(), std::default_random_engine(randSeed.fetch_add(1))); }, 
													[&](size_t topIdx, std::pair<SwapperLayers<Variables, bool>, std::default_random_engine>& swapperRand) {
			if(std::uniform_int_distribution<int>(0, topFraction)(swapperRand.second) != 0) return;

			const KeyValue<Monotonic<Variables>, ExtraData>& topKV = topLayer[topIdx];
			
			std::vector<Monotonic<Variables>> selectedBots;

			forEachBelowUsingSwapper(topLayerI, topIdx, allIntervalSizesAndDownLinks, swapperRand.first, [&](const KeyValue<Monotonic<Variables>, ExtraData>& botKV) {
				if(std::uniform_int_distribution<int>(0, botFraction)(swapperRand.second) != 0) return;

				selectedBots.push_back(botKV.key);
			});

			if(!selectedBots.empty()) {
				benchSetMutex.lock();
				TopBots<Variables> newElem;
				newElem.top = topKV.key;
				newElem.bots = std::move(selectedBots);
				benchSet.push_back(std::move(newElem));
				benchSetMutex.unlock();
			}
		});
	}

	std::default_random_engine generator;
	std::shuffle(benchSet.begin(), benchSet.end(), generator);

	std::ofstream benchmarkSetFile(FileName::benchmarkSetTopBots(Variables), std::ios::binary);
	if(!benchmarkSetFile.is_open()) throw "File not opened!";

	serializeVector(benchSet, benchmarkSetFile, serializeTopBots<Variables>);

	benchmarkSetFile.close();
}

template<unsigned int Variables>
void pipelineTestSet(size_t count) {
	std::vector<TopBots<Variables>> topBots = readTopBots<Variables>(5000);

	std::default_random_engine generator(std::chrono::high_resolution_clock::now().time_since_epoch().count());

	std::ofstream testSet(FileName::pipelineTestSet(Variables)); // plain text file

	const TopBots<Variables>& selectedTop = topBots[std::uniform_int_distribution<size_t>(0, topBots.size() - 1)(generator)];
	std::uniform_int_distribution<size_t> botSelector(0, selectedTop.bots.size() - 1);

	Monotonic<Variables> top = selectedTop.top;

	std::cout << "Starting generation" << std::endl;
	for(size_t i = 0; i < count; i++) {
		testSet << top.bf.bitset << '_';
		Monotonic<Variables> botA = selectedTop.bots[botSelector(generator)];
		Monotonic<Variables> botC = selectedTop.bots[botSelector(generator)];
		permuteRandom(botA, generator);
		permuteRandom(botC, generator);
		testSet << botA.bf.bitset << '_' << botC.bf.bitset << '_';
		uint64_t resultingBotSum = 
			computePCoefficientAllowBadBots<Variables>(top, botA) + 
			computePCoefficientAllowBadBots<Variables>(top, botC) + 
			computePCoefficientAllowBadBots<Variables>(top, botA.swapped(5,6)) + 
			computePCoefficientAllowBadBots<Variables>(top, botC.swapped(5,6));

		printBits(testSet, resultingBotSum, 64);
		testSet << std::endl;
	}

	testSet.close();
}

void pipeline24PackTestSet(size_t count) {
	constexpr unsigned int Variables = 7;
	std::vector<TopBots<Variables>> topBots = readTopBots<Variables>(5000);

	std::default_random_engine generator(std::chrono::high_resolution_clock::now().time_since_epoch().count());

	std::ofstream testSet(FileName::pipeline24PackTestSet(Variables)); // plain text file

	const TopBots<Variables>& selectedTop = topBots[std::uniform_int_distribution<size_t>(0, topBots.size() - 1)(generator)];
	std::uniform_int_distribution<size_t> botSelector(0, selectedTop.bots.size() - 1);

	Monotonic<Variables> top = selectedTop.top;
	permuteRandom(top, generator);

	std::cout << "Starting generation" << std::endl;
	for(size_t i = 0; i < count; i++) {
		testSet << top.bf.bitset << '_';
		Monotonic<Variables> bot = selectedTop.bots[botSelector(generator)];
		permuteRandom(bot, generator);
		testSet << bot.bf.bitset << '_';
		uint64_t resultingBotSum = 0;
		uint16_t resultingBotCount = 0;

		bot.forEachPermutation(3,7, [&](const Monotonic<Variables>& permBot) {
			if(permBot <= top) {
				resultingBotSum += computePCoefficientAllowBadBots(top, permBot);
				resultingBotCount++;
			}
		});

		printBits(testSet, resultingBotSum, 64);
		testSet << '_';
		printBits(testSet, resultingBotCount, 8);
		testSet << std::endl;
	}

	testSet.close();
}

CommandSet testSetCommands{"Test Set Generation", {
	{"benchmarkSample1", []() {benchmarkSample<1>(); }},
	{"benchmarkSample2", []() {benchmarkSample<2>(); }},
	{"benchmarkSample3", []() {benchmarkSample<3>(); }},
	{"benchmarkSample4", []() {benchmarkSample<4>(); }},
	{"benchmarkSample5", []() {benchmarkSample<5>(); }},
	{"benchmarkSample6", []() {benchmarkSample<6>(); }},
	{"benchmarkSample7", []() {benchmarkSample<7>(); }},

	{"benchmarkSampleTopBots1", []() {benchmarkSampleTopBots<1>(1,1); }},
	{"benchmarkSampleTopBots2", []() {benchmarkSampleTopBots<2>(1,1); }},
	{"benchmarkSampleTopBots3", []() {benchmarkSampleTopBots<3>(1,1); }},
	{"benchmarkSampleTopBots4", []() {benchmarkSampleTopBots<4>(1,1); }},
	{"benchmarkSampleTopBots5", []() {benchmarkSampleTopBots<5>(1,1); }},
	{"benchmarkSampleTopBots6", []() {benchmarkSampleTopBots<6>(100, 100); }},
	{"benchmarkSampleTopBots7", []() {benchmarkSampleTopBots<7>(10000, 10000); }},
},{

	{"pipelineTestSet1", [](const std::string& size) {pipelineTestSet<1>(std::stoi(size)); }},
	{"pipelineTestSet2", [](const std::string& size) {pipelineTestSet<2>(std::stoi(size)); }},
	{"pipelineTestSet3", [](const std::string& size) {pipelineTestSet<3>(std::stoi(size)); }},
	{"pipelineTestSet4", [](const std::string& size) {pipelineTestSet<4>(std::stoi(size)); }},
	{"pipelineTestSet5", [](const std::string& size) {pipelineTestSet<5>(std::stoi(size)); }},
	{"pipelineTestSet6", [](const std::string& size) {pipelineTestSet<6>(std::stoi(size)); }},
	{"pipelineTestSet7", [](const std::string& size) {pipelineTestSet<7>(std::stoi(size)); }},

	{"pipeline24PackTestSet", [](const std::string& size) {pipeline24PackTestSet(std::stoi(size)); }},
}};
