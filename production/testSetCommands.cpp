#include "commands.h"

#include "../dedelib/fileNames.h"
#include "../dedelib/allMBFsMap.h"
#include "../dedelib/intervalAndSymmetriesMap.h"
#include "../dedelib/toString.h"
#include "../dedelib/serialization.h"
#include "../dedelib/pcoeff.h"
#include "../dedelib/generators.h"

#include "../dedelib/flatMBFStructure.h"
#include "../dedelib/flatPCoeff.h"

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

void FullPermutePipelineTestSetOpenCL(std::vector<size_t> counts, std::string outFileMem, std::string outFileCpp) {
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
		printBits(testSet, 0, 16);
		testSet << '_';
		printBits(testSet, 0, 48);
		testSet << std::endl;

		testSetCpp << "{true, 0b"; printBits(testSetCpp, _mm_extract_epi64(top.bf.bitset.data, 1), 64);
		testSetCpp << ", 0b"; printBits(testSetCpp, _mm_extract_epi64(top.bf.bitset.data, 0), 64);
		testSetCpp << ", " << 0 << ", " << 0 << "}," << std::endl;
		std::cout << "Starting generation" << std::endl;

		BooleanFunction<Variables> tempGraphsBuf[factorial(Variables)];
		for(size_t i = 0; i < count; i++) {
			Monotonic<Variables> bot = selectedTop.bots[botSelector(generator)];
			permuteRandom(bot, generator);

			ProcessedPCoeffSum countConnectedSum = processPCoeffSum<Variables>(top, bot, tempGraphsBuf);

			uint64_t pcoeffSum = getPCoeffSum(countConnectedSum);
			uint64_t pcoeffCount = getPCoeffCount(countConnectedSum);

			testSet << "0_";
			testSet << bot.bf.bitset << '_';
			printBits(testSet, pcoeffCount, 16);
			testSet << '_';
			printBits(testSet, pcoeffSum, 48);
			testSet << std::endl;

			testSetCpp << "{false, 0b"; printBits(testSetCpp, _mm_extract_epi64(bot.bf.bitset.data, 1), 64);
			testSetCpp << ", 0b"; printBits(testSetCpp, _mm_extract_epi64(bot.bf.bitset.data, 0), 64);
			testSetCpp << ", " << pcoeffSum << ", " << pcoeffCount << "}," << std::endl;
		}
	}

	testSetCpp << "};" << std::endl;

	testSet.close();
	testSetCpp.close();
}

#include "../dedelib/flatPCoeffProcessing.h"

inline Monotonic<7> readMBFFromU64(const uint64_t* mbfsUINT64, NodeIndex idx) {
	NodeIndex mbfIndex = idx & 0x7FFFFFFF;
	uint64_t upper = mbfsUINT64[2*mbfIndex];
	uint64_t lower = mbfsUINT64[2*mbfIndex+1];
	Monotonic<7> mbf;
	mbf.bf.bitset.data = _mm_set_epi64x(upper, lower);
	return mbf;
} 

typedef std::uint32_t uint;
uint bitReverse4(uint v) {
  return ((v & 0x1) << 3) | ((v & 0x2) << 1) | ((v & 0x4) >> 1) | ((v & 0x8) >> 3);
}
// Very primitive shuffle for balancing out the load across a job. This divides the job into 16 chunks and intersperses them
uint shuffleIndex(uint idx, uint workGroupSize) {
  uint chunkOffset = workGroupSize >> 4;
  uint mod = idx & 0xF;
  uint div = idx >> 4;
  return chunkOffset * bitReverse4(mod) + div;
}
// Shuffles blocks of 16 elements instead of individual elements. For better memory utilization
uint shuffleClusters(uint idx, uint workGroupSize) {
  uint block = idx >> 4;
  uint idxInBlock = idx & 0xF;
  uint numberOfBlocks = workGroupSize >> 4;
  return (shuffleIndex(block, numberOfBlocks) << 4) | idxInBlock;
}

void GenTopsFullPermutePipelineTestSetOpenCL(std::vector<size_t> topsIn, std::string outFileMem, bool shuffle) {
	std::vector<NodeIndex> topsToProcess;
	for(size_t i = 0; i < topsIn.size(); i++) {
		topsToProcess.push_back(NodeIndex(topsIn[i]));
	}

	constexpr size_t Variables = 7;
	constexpr size_t BatchSize = 8;
	std::cout << "Reading FlatMBFStructure..." << std::endl;
	const FlatMBFStructure<Variables> allMBFData = readFlatMBFStructure<Variables>();
	const uint64_t* mbfsUINT64 = readFlatBufferNoMMAP<uint64_t>(FileName::flatMBFsU64(7), FlatMBFStructure<7>::MBF_COUNT * 2);
	std::cout << "FlatMBFStructure initialized." << std::endl;

	std::cout << "Starting Computation..." << std::endl;
	PCoeffProcessingContext context(Variables, topsToProcess.size(), topsToProcess.size(), topsToProcess.size());
	std::cout << "Input production..." << std::endl;
	inputProducer<Variables, BatchSize>(allMBFData, context, topsToProcess);
	std::cout << "Processing..." << std::endl;
	//std::thread cpuThread([&](){cpuProcessor_SingleThread(allMBFData, context);});

	std::cout << "Results..." << std::endl;

	std::ofstream testSet(outFileMem); // plain text file memory file

	std::cout << "Number of tops to process is " << topsToProcess.size() << std::endl;
	for(NodeIndex currentlyProcessingTopMeThinks : topsToProcess) {
		std::cout << "Waiting for top to process..." << std::endl;
		JobInfo job = context.inputQueue.pop_wait().value();
		NodeIndex top = job.bufStart[0] & 0x7FFFFFFF;
		size_t jobSize = job.size() & ~size_t(0xF);
		std::cout << "Processing top " << top << '/' << currentlyProcessingTopMeThinks << " of size " << jobSize << std::endl;
		Monotonic<Variables> topMBF = readMBFFromU64(mbfsUINT64, top);
		for(size_t elementI = 0; elementI < jobSize; elementI++) {
			size_t selectedElement = shuffle ? shuffleClusters(elementI, jobSize) : elementI;
			NodeIndex elem = job.bufStart[selectedElement];
			bool isTop = elementI == 0;
			//bool isTop = (uint32_t(elem) & 0x80000000) != 0;
			Monotonic<Variables> botMBF = readMBFFromU64(mbfsUINT64, elem);
			BooleanFunction<Variables> graphsBuf[factorial(Variables)];
			ProcessedPCoeffSum processed = processPCoeffSum(topMBF, botMBF, graphsBuf);

			testSet << (isTop ? '1' : '0');
			testSet << '_' << botMBF.bf.bitset << '_';
			printBits(testSet, getPCoeffCount(processed), 16);
			testSet << '_';
			printBits(testSet, getPCoeffSum(processed), 48);
			testSet << std::endl;
		}
		std::cout << "Finished processing top " << top << " of size " << jobSize << std::endl;
	}

	testSet.close();
	std::cout << "File written!" << std::endl;

	//context.outputQueue.close();
	//cpuThread.join();
}

void singleStreamPipelineTestSetOpenCL(std::vector<size_t> counts, std::string outFileMem) {
	constexpr unsigned int Variables = 7;
	std::vector<TopBots<Variables>> topBots = readTopBots<Variables>(5000);

	auto seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
	std::default_random_engine generator(seed);
	std::cout << "Seed: " << seed << std::endl;

	std::ofstream testSet(outFileMem); // plain text file memory file
	
	for(size_t topI = 0; topI < counts.size(); topI++) {
		size_t count = counts[topI];

		const TopBots<Variables>& selectedTop = topBots[std::uniform_int_distribution<size_t>(0, topBots.size() - 1)(generator)];
		std::uniform_int_distribution<size_t> botSelector(0, selectedTop.bots.size() - 1);

		Monotonic<Variables> top = selectedTop.top;
		permuteRandom(top, generator);
		testSet << "1_" << top.bf.bitset << '_';
		printBits(testSet, 0, 8);
		testSet << std::endl;

		std::cout << "Starting generation" << std::endl;
		for(size_t i = 0; i < count; ) {
			Monotonic<Variables> bot = selectedTop.bots[botSelector(generator)];
			permuteRandom(bot, generator);

			if(bot <= top) {
				uint64_t connectCount = countConnectedVeryFast(top.bf & ~bot.bf);

				testSet << "0_";
				testSet << bot.bf.bitset << '_';
				printBits(testSet, connectCount, 8);
				testSet << std::endl;

				i++;
			}
		}
	}

	testSet.close();
}


void permutCheck24TestSet() {
	constexpr unsigned int Variables = 7;

	FlatMBFStructure<Variables> flatMBFs = readFlatMBFStructure<Variables>(true, false, false, false);

	std::default_random_engine generator;

	std::ofstream testSetFile(FileName::permuteCheck24TestSet(Variables));

	for(int topLayer = 0; topLayer <= (1 << Variables); topLayer++) {
		std::cout << "Top " << topLayer << "/" << (1 << Variables) << std::endl;
		NodeOffset selectedTopIndex = std::uniform_int_distribution<int>(0, layerSizes[Variables][topLayer]-1)(generator);
		Monotonic<Variables> selectedTop = flatMBFs.mbfs[flatNodeLayerOffsets[Variables][topLayer] + selectedTopIndex];
		permuteRandom(selectedTop, generator);
		testSetFile << selectedTop.bf.bitset << "_" << selectedTop.bf.bitset << std::endl;
		// small offset to reduce generation time, by improving the odds that an element below will be picked
		for(int botLayer = 0; botLayer < topLayer - 2; botLayer++) {
			while(true) {
				NodeOffset selectedBotIndex = std::uniform_int_distribution<int>(0, layerSizes[Variables][botLayer]-1)(generator);
				Monotonic<Variables> selectedBot = flatMBFs.mbfs[flatNodeLayerOffsets[Variables][botLayer] + selectedBotIndex];

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





std::vector<size_t> parseSizeList(const std::vector<std::string>& strList) {
	std::vector<size_t> resultingCounts;
	resultingCounts.reserve(strList.size());

	for(const std::string& s : strList) {
		resultingCounts.push_back(std::stoi(s));
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

	{"pipelineTestSet1", [](const std::vector<std::string>& size) {pipelineTestSet<1>(std::stoi(size[0])); }},
	{"pipelineTestSet2", [](const std::vector<std::string>& size) {pipelineTestSet<2>(std::stoi(size[0])); }},
	{"pipelineTestSet3", [](const std::vector<std::string>& size) {pipelineTestSet<3>(std::stoi(size[0])); }},
	{"pipelineTestSet4", [](const std::vector<std::string>& size) {pipelineTestSet<4>(std::stoi(size[0])); }},
	{"pipelineTestSet5", [](const std::vector<std::string>& size) {pipelineTestSet<5>(std::stoi(size[0])); }},
	{"pipelineTestSet6", [](const std::vector<std::string>& size) {pipelineTestSet<6>(std::stoi(size[0])); }},
	{"pipelineTestSet7", [](const std::vector<std::string>& size) {pipelineTestSet<7>(std::stoi(size[0])); }},

	{"pipeline6TestSet", [](const std::vector<std::string>& size) {pipelinePackTestSet(std::stoi(size[0]), 4, FileName::pipeline6PackTestSet(7)); }},
	{"pipeline24PackTestSet", [](const std::vector<std::string>& size) {pipelinePackTestSet(std::stoi(size[0]), 3, FileName::pipeline24PackTestSet(7)); }},

	{"pipeline6TestSetOpenCL", [](const std::vector<std::string>& sizeList) {pipelinePackTestSetForOpenCL(parseSizeList(sizeList), 4, FileName::pipeline6PackTestSetForOpenCLMem(7), FileName::pipeline6PackTestSetForOpenCLCpp(7)); }},
	{"pipeline24PackTestSetOpenCL", [](const std::vector<std::string>& sizeList) {pipelinePackTestSetForOpenCL(parseSizeList(sizeList), 3, FileName::pipeline24PackTestSetForOpenCLMem(7), FileName::pipeline24PackTestSetForOpenCLCpp(7)); }},
	{"singleStreamPipelineTestSetOpenCL", [](const std::vector<std::string>& sizeList) {singleStreamPipelineTestSetOpenCL(parseSizeList(sizeList), FileName::singleStreamPipelineTestSetForOpenCLMem(7)); }},

	{"FullPermutePipelineTestSetOpenCL", [](const std::vector<std::string>& sizeList) {FullPermutePipelineTestSetOpenCL(parseSizeList(sizeList), FileName::FullPermutePipelineTestSetOpenCLMem(7), FileName::FullPermutePipelineTestSetOpenCLCpp(7)); }},

	{"GenTopsFullPermutePipelineTestSetOpenCL", [](const std::vector<std::string>& topList) {GenTopsFullPermutePipelineTestSetOpenCL(parseSizeList(topList), FileName::FullPermutePipelineTestSetOpenCLMem(7), true);}}
}};
