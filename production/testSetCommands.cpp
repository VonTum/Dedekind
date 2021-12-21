#include "commands.h"

#include "../dedelib/fileNames.h"
#include "../dedelib/allMBFsMap.h"
#include "../dedelib/intervalAndSymmetriesMap.h"
#include "../dedelib/toString.h"
#include "../dedelib/serialization.h"
#include "../dedelib/pcoeff.h"

#include "../dedelib/flatMBFStructure.h"
#include "../dedelib/flatPCoeff.h"

#include <random>
#include <sstream>


template<unsigned int Variables, typename RandomEngine>
void permuteRandom(BooleanFunction<Variables>& bf, RandomEngine& generator, unsigned int from = 0, unsigned int to = Variables) {
	for(unsigned int i = from; i < to; i++) {
		unsigned int selectedIndex = std::uniform_int_distribution<unsigned int>(i, to - 1)(generator);
		if(selectedIndex == i) continue; // leave i where it is
		bf.swap(i, selectedIndex); // put selectedIndex in position i
	}
}

template<unsigned int Variables, typename RandomEngine>
void permuteRandom(AntiChain<Variables>& ac, RandomEngine& generator, unsigned int from = 0, unsigned int to = Variables) {
	permuteRandom<Variables, RandomEngine>(ac.bf, generator, from, to);
}

template<unsigned int Variables, typename RandomEngine>
void permuteRandom(Monotonic<Variables>& mbf, RandomEngine& generator, unsigned int from = 0, unsigned int to = Variables) {
	permuteRandom<Variables, RandomEngine>(mbf.bf, generator, from, to);
}

template<unsigned int Variables, typename RandomEngine>
void permuteRandom(Layer<Variables>& layer, RandomEngine& generator, unsigned int from = 0, unsigned int to = Variables) {
	permuteRandom<Variables, RandomEngine>(layer.bf, generator, from, to);
}




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

void pipelinePackTestSet(size_t count, unsigned int permuteStartVar, std::string outFile) {
	constexpr unsigned int Variables = 7;
	std::vector<TopBots<Variables>> topBots = readTopBots<Variables>(5000);

	auto seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
	std::default_random_engine generator(seed);
	std::cout << "Seed: " << seed << std::endl;

	std::ofstream testSet(outFile); // plain text file

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

		bot.forEachPermutation(permuteStartVar, 7, [&](const Monotonic<Variables>& permBot) {
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


void pipelinePackTestSetForOpenCL(std::vector<size_t> counts, unsigned int permuteStartVar, std::string outFileMem, std::string outFileCpp) {
	constexpr unsigned int Variables = 7;
	std::vector<TopBots<Variables>> topBots = readTopBots<Variables>(5000);

	auto seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
	std::default_random_engine generator(seed);
	std::cout << "Seed: " << seed << std::endl;

	std::ofstream testSet(outFileMem); // plain text file memory file
	std::ofstream testSetCpp(outFileCpp); // plain text file cpp file

	testSetCpp << "#include \"dataFormat.h\"" << std::endl;
	testSetCpp << "std::vector<DataItem> allData{" << std::endl;

	for(size_t topI = 0; topI < counts.size(); topI++) {
		size_t count = counts[topI];

		const TopBots<Variables>& selectedTop = topBots[std::uniform_int_distribution<size_t>(0, topBots.size() - 1)(generator)];
		std::uniform_int_distribution<size_t> botSelector(0, selectedTop.bots.size() - 1);

		Monotonic<Variables> top = selectedTop.top;
		permuteRandom(top, generator);
		testSet << "1_" << top.bf.bitset << '_';
		printBits(testSet, 0, 64);
		testSet << '_';
		printBits(testSet, 0, 8);
		testSet << std::endl;

		testSetCpp << "{true, 0b"; printBits(testSetCpp, _mm_extract_epi64(top.bf.bitset.data, 1), 64);
		testSetCpp << ", 0b"; printBits(testSetCpp, _mm_extract_epi64(top.bf.bitset.data, 0), 64);
		testSetCpp << ", " << 0 << ", " << 0 << "}," << std::endl;
		std::cout << "Starting generation" << std::endl;
		for(size_t i = 0; i < count; i++) {
			Monotonic<Variables> bot = selectedTop.bots[botSelector(generator)];
			permuteRandom(bot, generator);
			uint64_t resultingBotSum = 0;
			uint16_t resultingBotCount = 0;

			bot.forEachPermutation(permuteStartVar, 7, [&](const Monotonic<Variables>& permBot) {
				if(permBot <= top) {
					resultingBotSum += computePCoefficientAllowBadBots(top, permBot);
					resultingBotCount++;
				}
			});

			testSet << "0_";
			testSet << bot.bf.bitset << '_';
			printBits(testSet, resultingBotSum, 64);
			testSet << '_';
			printBits(testSet, resultingBotCount, 8);
			testSet << std::endl;

			testSetCpp << "{false, 0b"; printBits(testSetCpp, _mm_extract_epi64(bot.bf.bitset.data, 1), 64);
			testSetCpp << ", 0b"; printBits(testSetCpp, _mm_extract_epi64(bot.bf.bitset.data, 0), 64);
			testSetCpp << ", " << resultingBotSum << ", " << resultingBotCount << "}," << std::endl;
		}
	}

	testSetCpp << "};" << std::endl;

	testSet.close();
	testSetCpp.close();
}


void permutCheck24TestSet() {
	constexpr unsigned int Variables = 7;

	FlatMBFStructure<Variables> flatMBFs = readFlatMBFStructure<Variables>(true, false, false, false);

	std::default_random_engine generator;

	std::ofstream testSetFile(FileName::permuteCheck24TestSet(Variables));

	for(int topLayer = 0; topLayer <= (1 << Variables); topLayer++) {
		std::cout << "Top " << topLayer << "/" << (1 << Variables) << std::endl;
		NodeOffset selectedTopIndex = std::uniform_int_distribution<int>(0, getLayerSize<Variables>(topLayer)-1)(generator);
		Monotonic<Variables> selectedTop = flatMBFs.mbfs[FlatMBFStructure<Variables>::cachedOffsets[topLayer] + selectedTopIndex];
		permuteRandom(selectedTop, generator);
		testSetFile << selectedTop.bf.bitset << "_" << selectedTop.bf.bitset << std::endl;
		// small offset to reduce generation time, by improving the odds that an element below will be picked
		for(int botLayer = 0; botLayer < topLayer - 2; botLayer++) {
			while(true) {
				NodeOffset selectedBotIndex = std::uniform_int_distribution<int>(0, getLayerSize<Variables>(botLayer)-1)(generator);
				Monotonic<Variables> selectedBot = flatMBFs.mbfs[FlatMBFStructure<Variables>::cachedOffsets[botLayer] + selectedBotIndex];

				if(hasPermutationBelow(selectedTop, selectedBot)) {
					permuteRandom(selectedBot, generator, 3);
					testSetFile << selectedTop.bf.bitset << "_" << selectedBot.bf.bitset << std::endl;
					break;
				}
				// otherwise keep searching
			}
		}
	}
	testSetFile.close();
}

/*
	Parses "38431,31,13854,111,3333" to {38431,31,13854,111,3333}
*/
std::vector<size_t> parseSizeList(const std::string& strList) {
	std::stringstream strStream(strList);
	std::string segment;
	std::vector<size_t> resultingCounts;

	while(std::getline(strStream, segment, ',')) {
		resultingCounts.push_back(std::stoi(segment));
	}

	return resultingCounts;
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

	{"permutCheck24TestSet", []() {permutCheck24TestSet(); }},
},{

	{"pipelineTestSet1", [](const std::string& size) {pipelineTestSet<1>(std::stoi(size)); }},
	{"pipelineTestSet2", [](const std::string& size) {pipelineTestSet<2>(std::stoi(size)); }},
	{"pipelineTestSet3", [](const std::string& size) {pipelineTestSet<3>(std::stoi(size)); }},
	{"pipelineTestSet4", [](const std::string& size) {pipelineTestSet<4>(std::stoi(size)); }},
	{"pipelineTestSet5", [](const std::string& size) {pipelineTestSet<5>(std::stoi(size)); }},
	{"pipelineTestSet6", [](const std::string& size) {pipelineTestSet<6>(std::stoi(size)); }},
	{"pipelineTestSet7", [](const std::string& size) {pipelineTestSet<7>(std::stoi(size)); }},

	{"pipeline6TestSet", [](const std::string& size) {pipelinePackTestSet(std::stoi(size), 4, FileName::pipeline6PackTestSet(7)); }},
	{"pipeline24PackTestSet", [](const std::string& size) {pipelinePackTestSet(std::stoi(size), 3, FileName::pipeline24PackTestSet(7)); }},

	{"pipeline6TestSetOpenCL", [](const std::string& sizeList) {pipelinePackTestSetForOpenCL(parseSizeList(sizeList), 4, FileName::pipeline6PackTestSetForOpenCLMem(7), FileName::pipeline6PackTestSetForOpenCLCpp(7)); }},
	{"pipeline24PackTestSetOpenCL", [](const std::string& sizeList) {pipelinePackTestSetForOpenCL(parseSizeList(sizeList), 3, FileName::pipeline24PackTestSetForOpenCLMem(7), FileName::pipeline24PackTestSetForOpenCLCpp(7)); }},
}};
