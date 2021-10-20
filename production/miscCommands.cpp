#include "commands.h"

#include <iostream>
#include <fstream>
#include <random>
#include <map>
#include <string>
#include <chrono>
#include "../dedelib/dedekindDecomposition.h"
#include "../dedelib/valuedDecomposition.h"
#include "../dedelib/toString.h"
#include "../dedelib/serialization.h"

#include "../dedelib/codeGen.h"

#include "../dedelib/MBFDecomposition.h"
#include "../dedelib/r8Computation.h"
#include "../dedelib/tjomn.h"
#include "../dedelib/sjomn.h"

#include "../dedelib/fullIntervalSizeComputation.h"
#include "../dedelib/intervalAndSymmetriesMap.h"
#include "../dedelib/pcoeff.h"

#include "../dedelib/fileNames.h"

#include "../dedelib/allMBFsMap.h"

#include "../dedelib/timeTracker.h"

void RAMTestBenchFunc(size_t numHops, size_t numCurs, uint32_t* curs, uint32_t* jumpTable) {
	for(size_t i = 0; i < numHops; i++) {
		/*for(size_t c = 0; c < numCurs; c++) {
			#ifdef _MSC_VER
				_mm_prefetch(reinterpret_cast<const char*>(jumpTable + curs[c]), _MM_HINT_ENTA);
			#else
				_mm_prefetch(reinterpret_cast<const char*>(jumpTable + curs[c]), _MM_HINT_NTA);
			#endif
		}*/
		for(size_t c = 0; c < numCurs; c++) {
			curs[c] = jumpTable[curs[c]];
		}
	}
}

void doRAMTest() {
	std::cout << "RAM test\n";
	constexpr size_t numCurs = 32;
	constexpr std::size_t size = std::size_t(1) << 28;

	std::cout << "Making random generator\n";
	std::default_random_engine generator;
	std::uniform_int_distribution<uint32_t> distribution(0, size - 1);

	std::cout << "Init jump table size " << size << "\n";
	uint32_t* jumpTable = new uint32_t[size];
	for(uint32_t i = 0; i < size; i++) jumpTable[i] = 0;
	{
		// create big cycle-free chain
		uint32_t curHead = 0;
		for(uint32_t i = 0; i < size / 2;) {
			uint32_t nextJump = distribution(generator);
			if(jumpTable[nextJump] == 0) {
				jumpTable[curHead] = nextJump;
				curHead = nextJump;
				i++;
			}
		}
		jumpTable[curHead] = 0;
	}
	
	std::cout << "Init cursors numCurs=" << numCurs << "\n";
	uint32_t curs[numCurs];
	for(size_t i = 0; i < numCurs; ) {
		uint32_t selected = distribution(generator);
		if(jumpTable[selected] != 0) {
			curs[i] = selected;
			i++;
		}
	}
	constexpr size_t numHops = 1ULL << 26;
	std::cout << "Starting for " << numHops << " hops\n";
	std::chrono::nanoseconds finalTime;
	{
		TimeTracker timer;

		RAMTestBenchFunc(numHops, numCurs, curs, jumpTable);

		finalTime = timer.getTime();
	}

	size_t totalNumHops = numHops * numCurs;
	for(size_t i = 0; i < numCurs; i++) {
		std::cout << "final value " << curs[i] << "\n";
	}
	std::cout << totalNumHops << " hops!\n";
	std::cout << double(totalNumHops) / finalTime.count() * 1000000000 << " hops per second\n";
	std::cout << finalTime.count() / double(totalNumHops) << "ns/hop\n";
}


template<unsigned int Variables>
void doLinkCount() {
	size_t linkCounts[(1 << Variables) + 1];

	std::ifstream linkFile(FileName::mbfLinks(Variables), std::ios::binary);
	std::ofstream linkStatsFile(FileName::linkStats(Variables));

	size_t numLinksDistri[36];
	for(size_t& item : numLinksDistri) item = 0;

	size_t linkSizeDistri[36];
	for(size_t& item : linkSizeDistri) item = 0;

	char buf[4 * 35];
	for(size_t layer = 0; layer < (1 << Variables) + 1; layer++) {
		int maxNumLinks = 0;
		size_t maxLinkCount = 0;
		size_t totalLinks = 0;
		for(size_t itemInLayer = 0; itemInLayer < getLayerSize<Variables>(layer); itemInLayer++) {
			std::streamsize numLinks = static_cast<std::streamsize>(linkFile.get());
			numLinksDistri[numLinks]++;
			if(numLinks > maxNumLinks) maxNumLinks = numLinks;
			totalLinks += numLinks;

			linkFile.read(buf, 4 * numLinks);

			for(std::streamsize i = 0; i < numLinks; i++) {
				size_t linkSize = buf[4 * i];
				linkSizeDistri[linkSize]++;
				if(linkSize > maxLinkCount) {
					maxLinkCount = linkSize;
				}
			}
		}

		linkCounts[layer] = totalLinks;
		std::cout << "layer: " << layer << " maxNumLinks: " << maxNumLinks << " maxLinkCount: " << maxLinkCount << " totalLinks: " << totalLinks << "\n";
		linkStatsFile << "layer: " << layer << " maxNumLinks: " << maxNumLinks << " maxLinkCount: " << maxLinkCount << " totalLinks: " << totalLinks << "\n";
	}

	std::cout << "Link Num Distribution:\n";
	linkStatsFile << "Link Num Distribution:\n";
	for(int i = 1; i < 36; i++) {
		std::cout << i << ": " << numLinksDistri[i] << "\n";
		linkStatsFile << i << ": " << numLinksDistri[i] << "\n";
	}

	std::cout << "Link Size Distribution:\n";
	linkStatsFile << "Link Size Distribution:\n";
	for(int i = 1; i < 36; i++) {
		std::cout << i << ": " << linkSizeDistri[i] << "\n";
		linkStatsFile << i << ": " << linkSizeDistri[i] << "\n";
	}

	std::cout << "Link Counts summary:\n" << linkCounts[0];
	linkStatsFile << "Link Counts summary:\n" << linkCounts[0];
	for(size_t layer = 1; layer < (1 << Variables); layer++) {
		std::cout << ", " << linkCounts[layer];
		linkStatsFile << ", " << linkCounts[layer];
	}
	std::cout << "\n";
	linkStatsFile << "\n";

	linkFile.close();
	linkStatsFile.close();

}

template<unsigned int Variables>
void sampleIntervalSizes() {
	std::ofstream intervalStatsFile;
	/*std::ofstream intervalStatsFile(intervalStats(Variables));

	intervalStatsFile << "layer, intervalSize";
	for(size_t layer = 0; layer < (1 << Variables); layer++) {
		intervalStatsFile << ", " << getLayerSize<Variables>(layer);
	}
	intervalStatsFile << ", totalCount\n";
	intervalStatsFile.close();*/
	std::default_random_engine generator;
	for(size_t layer = 49; layer < (1 << Variables); layer++) {
		std::uniform_int_distribution<unsigned int> random(0, getLayerSize<Variables>(layer) - 1);
		unsigned int selectedInLayer = random(generator);
		std::cout << selectedInLayer << "\n";
		//readAllLinks<Variables>();
		std::pair<std::array<uint64_t, (1 << Variables)>, uint64_t> countAndIntervalSize = computeIntervalSize<Variables>(layer, selectedInLayer);
		std::array<uint64_t, (1 << Variables)> counts = countAndIntervalSize.first;
		uint64_t iSize = countAndIntervalSize.second;

		intervalStatsFile.open(FileName::intervalStats(Variables), std::ios::app);
		intervalStatsFile << layer << ", " << iSize;

		uint64_t totalCount = 0;
		for(uint64_t count : counts) {
			totalCount += count;
			intervalStatsFile << ", " << count;
		}
		intervalStatsFile << ", " << totalCount << "\n";
		intervalStatsFile.close();
	}

}

/*
	DOES NOT WORK
*/
template<unsigned int Variables>
uint64_t getIntervalSizeForFast(const MBFDecomposition<Variables>& dec, int nodeLayer, int nodeIndex) {
	SwapperLayers<Variables, int> swapper;
	swapper.dest(nodeIndex) += 1;
	swapper.pushNext();

	BooleanFunction<Variables> start = dec.get(nodeLayer, nodeIndex);

	uint64_t intervalSize = 0;

	for(int layer = nodeLayer; layer < (1 << Variables); layer++) {
		//std::cout << "checking layer " << i << "\n";

		for(int dirtyIndex : swapper) {
			int count = swapper[dirtyIndex];

			//std::cout << "  dirty index " << dirtyIndex << "\n";

			intervalSize += count;

			BooleanFunction<Variables> cur = dec.get(layer, dirtyIndex);

			for(LinkedNode& ln : dec.iterLinksOf(layer, dirtyIndex)){
				//std::cout << "    subLink " << ln.count << "x" << ln.index << "\n";

				BooleanFunction<Variables> to = dec.get(layer+1, ln.index);

				unsigned int modifiedLayer = getModifiedLayer(cur, to);

				uint64_t multiplier = getFormationCountWithout(to, start, modifiedLayer);

				swapper.dest(ln.index) += ln.count * count * multiplier;
			}
		}
		swapper.pushNext();
		for(int dirtyIndex : swapper) {
			BooleanFunction<Variables> to = dec.get(layer + 1, dirtyIndex);
			int divideBy = getFormationCount(to, start);
			assert(swapper[dirtyIndex] % divideBy == 0);
			swapper[dirtyIndex] /= divideBy;
		}
	}

	return intervalSize;
}

template<unsigned int Variables>
int countSuperSetPermutations(const BooleanFunction<Variables>& funcToPermute, const BooleanFunction<Variables>& superSet) {
	int totalFound = 0;
	int duplicates = 0;
	
	BooleanFunction<Variables> permut = funcToPermute;

	permut.forEachPermutation([&](const BooleanFunction<Variables>& permuted) {
		if(superSet.isSubSetOf(permuted)) {
			totalFound++;
		}
		if(permuted == funcToPermute) {
			duplicates++;
		}
	});

	assert(totalFound >= 1);
	assert(duplicates >= 1);
	assert(totalFound % duplicates == 0);
	return totalFound / duplicates;
}

template<unsigned int Variables>
std::pair<uint64_t, uint64_t> getIntervalSizeFor(const MBFDecomposition<Variables>& dec, int nodeLayer, int nodeIndex) {
	SwapperLayers<Variables, bool> swapper;
	swapper.set(nodeIndex);
	swapper.pushNext();

	Monotonic<Variables> start = dec.get(nodeLayer, nodeIndex);

	uint64_t intervalSize = 0;
	uint64_t uniqueClassesSize = 0;

	for(int layer = nodeLayer; layer < (1 << Variables); layer++) {
		//std::cout << "checking layer " << i << "\n";

		for(int dirtyIndex : swapper) {
			//std::cout << "  dirty index " << dirtyIndex << "\n";

			Monotonic<Variables> cur = dec.get(layer, dirtyIndex);

			int numSubSets = countSuperSetPermutations(cur.bf, start.bf);
			if(numSubSets < 1) {
				//__debugbreak();
				int numSubSets = countSuperSetPermutations(cur.bf, start.bf);
			}

			intervalSize += numSubSets;
			uniqueClassesSize++;

			for(LinkedNode& ln : dec.iterLinksOf(layer, dirtyIndex)) {
				//std::cout << "    subLink " << ln.count << "x" << ln.index << "\n";

				Monotonic<Variables> to = dec.get(layer + 1, ln.index);

				swapper.set(ln.index);
			}
		}
		swapper.pushNext();
	}
	intervalSize += 1;
	uniqueClassesSize += 1;

	return std::make_pair(intervalSize, uniqueClassesSize);
}

template<unsigned int Variables>
void doRevolution() {
	std::cout << "Computing D(" << Variables << ")...\n";

	TimeTracker timer;

	revolutionParallel<Variables - 3>();
}


template<unsigned int Variables>
void benchPCoeffLayerElementStats(size_t topLayer) {
	std::atomic<bool> shouldStop(false);
	std::thread diagThread([&]() {
		while(!shouldStop) {
			printHistogramAndPCoeffs(Variables);
			std::this_thread::sleep_for(std::chrono::seconds(10));
		}
	});
	pcoeffLayerElementStats<Variables>(topLayer);
	shouldStop = true;
	diagThread.join();
}


template<unsigned int Variables>
void findLongestMiddleLayerChainRecurseUp(BooleanFunction<Variables> curChain, size_t newestElement, BooleanFunction<Variables>& longestChain, size_t& longestChainLength) {
	Layer<Variables> newPossibleElements = Layer<Variables>{newestElement}.pred();
	BooleanFunction<Variables> blockedElements = curChain.monotonizeDown();
	BooleanFunction<Variables> chooseableElements = newPossibleElements.bf & ~blockedElements;
	curChain.add(newestElement);

	size_t curChainSize = curChain.size();
	if(curChainSize > longestChainLength) {
		longestChainLength = curChainSize;
		longestChain = curChain;
		std::cout << "Better chain found: (Length " << longestChainLength << ") " << longestChain << std::endl;
	}

	chooseableElements.forEachOne([curChain, &longestChain, &longestChainLength](size_t newElement){
		findLongestMiddleLayerChainRecurseDown(curChain, newElement, longestChain, longestChainLength);
	});
}
template<unsigned int Variables>
void findLongestMiddleLayerChainRecurseDown(BooleanFunction<Variables> curChain, size_t newestElement, BooleanFunction<Variables>& longestChain, size_t& longestChainLength) {
	Layer<Variables> newPossibleElements = Layer<Variables>{newestElement}.succ();
	BooleanFunction<Variables> blockedElements = curChain.monotonizeUp();
	BooleanFunction<Variables> chooseableElements = newPossibleElements.bf & ~blockedElements;
	curChain.add(newestElement);

	size_t curChainSize = curChain.size();
	if(curChainSize > longestChainLength) {
		longestChainLength = curChainSize;
		longestChain = curChain;
		std::cout << "Better chain found: (Length " << longestChainLength << ") " << longestChain << std::endl;
	}

	chooseableElements.forEachOne([curChain, &longestChain, &longestChainLength](size_t newElement){
		findLongestMiddleLayerChainRecurseUp(curChain, newElement, longestChain, longestChainLength);
	});
}

template<unsigned int Variables>
BooleanFunction<Variables> findLongestMiddleLayerChain() {
	Layer<Variables> middleLayerA(BooleanFunction<Variables>::layerMask(Variables / 2));
	Layer<Variables> middleLayerB(BooleanFunction<Variables>::layerMask(Variables / 2 + 1));

	BooleanFunction<Variables> curChain = BooleanFunction<Variables>::empty();
	curChain.add(middleLayerA.getFirst());
	size_t newestIndex = (curChain.monotonizeUp() & middleLayerB).getFirst();
	
	BooleanFunction<Variables> longestChain = curChain;
	size_t longestChainLength = longestChain.size();
	std::cout << "Starting best chain: (Length " << longestChainLength << ") " << longestChain << std::endl;
	findLongestMiddleLayerChainRecurseUp(curChain, newestIndex, longestChain, longestChainLength);

	std::cout << "Longest chain found: (Length " << longestChainLength << ") " << longestChain << std::endl;

	return longestChain;
}

template<unsigned int Variables, typename RandomEngine>
void permuteRandom(Monotonic<Variables>& mbf, RandomEngine& generator) {
	for(unsigned int i = 0; i < Variables; i++) {
		unsigned int selectedIndex = std::uniform_int_distribution<unsigned int>(i, Variables - 1)(generator);
		if(selectedIndex == i) continue; // leave i where it is
		mbf.swap(i, selectedIndex); // put selectedIndex in position i
	}
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

CommandSet miscCommands{"Misc", {
	{"ramTest", []() {doRAMTest(); }},

	{"graphVis1", []() {genGraphVisCode(1); }},
	{"graphVis2", []() {genGraphVisCode(2); }},
	{"graphVis3", []() {genGraphVisCode(3); }},
	{"graphVis4", []() {genGraphVisCode(4); }},
	{"graphVis5", []() {genGraphVisCode(5); }},
	{"graphVis6", []() {genGraphVisCode(6); }},
	{"graphVis7", []() {genGraphVisCode(7); }},
	{"graphVis8", []() {genGraphVisCode(8); }},
	{"graphVis9", []() {genGraphVisCode(9); }},

	{"revolution4", []() {doRevolution<4>(); }},
	{"revolution5", []() {doRevolution<5>(); }},
	{"revolution6", []() {doRevolution<6>(); }},
	{"revolution7", []() {doRevolution<7>(); }},
	{"revolution8", []() {doRevolution<8>(); }},
	{"revolution9", []() {doRevolution<9>(); }},

	{"linkCount1", []() {doLinkCount<1>(); }},
	{"linkCount2", []() {doLinkCount<2>(); }},
	{"linkCount3", []() {doLinkCount<3>(); }},
	{"linkCount4", []() {doLinkCount<4>(); }},
	{"linkCount5", []() {doLinkCount<5>(); }},
	{"linkCount6", []() {doLinkCount<6>(); }},
	{"linkCount7", []() {doLinkCount<7>(); }},
	//{"linkCount8", []() {doLinkCount<8>(); }},
	//{"linkCount9", []() {doLinkCount<9>(); }},

	{"findLongestMiddleLayerChain1", []() {findLongestMiddleLayerChain<1>(); }},
	{"findLongestMiddleLayerChain2", []() {findLongestMiddleLayerChain<2>(); }},
	{"findLongestMiddleLayerChain3", []() {findLongestMiddleLayerChain<3>(); }},
	{"findLongestMiddleLayerChain4", []() {findLongestMiddleLayerChain<4>(); }},
	{"findLongestMiddleLayerChain5", []() {findLongestMiddleLayerChain<5>(); }},
	{"findLongestMiddleLayerChain6", []() {findLongestMiddleLayerChain<6>(); }},
	{"findLongestMiddleLayerChain7", []() {findLongestMiddleLayerChain<7>(); }},
	{"findLongestMiddleLayerChain8", []() {findLongestMiddleLayerChain<8>(); }},
	{"findLongestMiddleLayerChain9", []() {findLongestMiddleLayerChain<9>(); }},

	//{"sampleIntervalSizes1", []() {sampleIntervalSizes<1>(); }},
	//{"sampleIntervalSizes2", []() {sampleIntervalSizes<2>(); }},
	//{"sampleIntervalSizes3", []() {sampleIntervalSizes<3>(); }},
	//{"sampleIntervalSizes4", []() {sampleIntervalSizes<4>(); }},
	//{"sampleIntervalSizes5", []() {sampleIntervalSizes<5>(); }},
	//{"sampleIntervalSizes6", []() {sampleIntervalSizes<6>(); }},
	//{"sampleIntervalSizes7", []() {sampleIntervalSizes<7>(); }},
	//{"sampleIntervalSizes8", []() {sampleIntervalSizes<8>(); }},
	//{"sampleIntervalSizes9", []() {sampleIntervalSizes<9>(); }},

	{"verifyIntervalsCorrect1", []() {verifyIntervalsCorrect<1>(); }},
	{"verifyIntervalsCorrect2", []() {verifyIntervalsCorrect<2>(); }},
	{"verifyIntervalsCorrect3", []() {verifyIntervalsCorrect<3>(); }},
	{"verifyIntervalsCorrect4", []() {verifyIntervalsCorrect<4>(); }},
	{"verifyIntervalsCorrect5", []() {verifyIntervalsCorrect<5>(); }},
	{"verifyIntervalsCorrect6", []() {verifyIntervalsCorrect<6>(); }},
	{"verifyIntervalsCorrect7", []() {verifyIntervalsCorrect<7>(); }},
	//{"verifyIntervalsCorrect8", []() {verifyIntervalsCorrect<8>(); }},
	//{"verifyIntervalsCorrect9", []() {verifyIntervalsCorrect<9>(); }},

	{"computeDPlusOne1", []() {computeDPlus1<1>(); }},
	{"computeDPlusOne2", []() {computeDPlus1<2>(); }},
	{"computeDPlusOne3", []() {computeDPlus1<3>(); }},
	{"computeDPlusOne4", []() {computeDPlus1<4>(); }},
	{"computeDPlusOne5", []() {computeDPlus1<5>(); }},
	{"computeDPlusOne6", []() {computeDPlus1<6>(); }},
	{"computeDPlusOne7", []() {computeDPlus1<7>(); }},
	//{"computeDedekindFromIntervals8", []() {computeDPlus1<8>(); }},
	//{"computeDedekindFromIntervals9", []() {computeDPlus1<9>(); }},

	{"noCanonizationPCoeffMethod1", []() {noCanonizationPCoeffMethod<1>(); }},
	{"noCanonizationPCoeffMethod2", []() {noCanonizationPCoeffMethod<2>(); }},
	{"noCanonizationPCoeffMethod3", []() {noCanonizationPCoeffMethod<3>(); }},
	{"noCanonizationPCoeffMethod4", []() {noCanonizationPCoeffMethod<4>(); }},
	{"noCanonizationPCoeffMethod5", []() {noCanonizationPCoeffMethod<5>(); }},
	{"noCanonizationPCoeffMethod6", []() {noCanonizationPCoeffMethod<6>(); }},
	{"noCanonizationPCoeffMethod7", []() {noCanonizationPCoeffMethod<7>(); }},

	{"newPCoeff1", []() {pcoeffMethodV2<1>(); }},
	{"newPCoeff2", []() {pcoeffMethodV2<2>(); }},
	{"newPCoeff3", []() {pcoeffMethodV2<3>(); }},
	{"newPCoeff4", []() {pcoeffMethodV2<4>(); }},
	{"newPCoeff5", []() {pcoeffMethodV2<5>(); }},
	{"newPCoeff6", []() {pcoeffMethodV2<6>(); }},
	{"newPCoeff7", []() {pcoeffMethodV2<7>(); }}, 

	{"pcoeffTimeEstimate1", []() {pcoeffTimeEstimate<1>(); }},
	{"pcoeffTimeEstimate2", []() {pcoeffTimeEstimate<2>(); }},
	{"pcoeffTimeEstimate3", []() {pcoeffTimeEstimate<3>(); }},
	{"pcoeffTimeEstimate4", []() {pcoeffTimeEstimate<4>(); }},
	{"pcoeffTimeEstimate5", []() {pcoeffTimeEstimate<5>(); }},
	{"pcoeffTimeEstimate6", []() {pcoeffTimeEstimate<6>(); }},
	{"pcoeffTimeEstimate7", []() {pcoeffTimeEstimate<7>(); }},

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

	{"computeIntervalSizesNaive1", []() {computeIntervalSizesNaive<1>(); }},
	{"computeIntervalSizesNaive2", []() {computeIntervalSizesNaive<2>(); }},
	{"computeIntervalSizesNaive3", []() {computeIntervalSizesNaive<3>(); }},
	{"computeIntervalSizesNaive4", []() {computeIntervalSizesNaive<4>(); }},
	{"computeIntervalSizesNaive5", []() {computeIntervalSizesNaive<5>(); }},
	{"computeIntervalSizesNaive6", []() {computeIntervalSizesNaive<6>(); }},
	{"computeIntervalSizesNaive7", []() {computeIntervalSizesNaive<7>(); }},

	{"computeIntervalSizesFast1", []() {computeIntervalSizesFast<1>(); }},
	{"computeIntervalSizesFast2", []() {computeIntervalSizesFast<2>(); }},
	{"computeIntervalSizesFast3", []() {computeIntervalSizesFast<3>(); }},
	{"computeIntervalSizesFast4", []() {computeIntervalSizesFast<4>(); }},
	{"computeIntervalSizesFast5", []() {computeIntervalSizesFast<5>(); }},
	{"computeIntervalSizesFast6", []() {computeIntervalSizesFast<6>(); }},
	{"computeIntervalSizesFast7", []() {computeIntervalSizesFast<7>(); }},
}, {
	{"checkIntervalLayers1", [](const std::string& size) {checkIntervalLayers<1>(std::stoi(size)); }},
	{"checkIntervalLayers2", [](const std::string& size) {checkIntervalLayers<2>(std::stoi(size)); }},
	{"checkIntervalLayers3", [](const std::string& size) {checkIntervalLayers<3>(std::stoi(size)); }},
	{"checkIntervalLayers4", [](const std::string& size) {checkIntervalLayers<4>(std::stoi(size)); }},
	{"checkIntervalLayers5", [](const std::string& size) {checkIntervalLayers<5>(std::stoi(size)); }},
	{"checkIntervalLayers6", [](const std::string& size) {checkIntervalLayers<6>(std::stoi(size)); }},
	{"checkIntervalLayers7", [](const std::string& size) {checkIntervalLayers<7>(std::stoi(size)); }},
	//{"checkIntervalLayers8", [](const std::string& size) {checkIntervalLayers<8>(std::stoi(size)); }},
	//{"checkIntervalLayers9", [](const std::string& size) {checkIntervalLayers<9>(std::stoi(size)); }}

	{"pcoeffLayerElementStats1", [](const std::string& size) {benchPCoeffLayerElementStats<1>(std::stoi(size)); }},
	{"pcoeffLayerElementStats2", [](const std::string& size) {benchPCoeffLayerElementStats<2>(std::stoi(size)); }},
	{"pcoeffLayerElementStats3", [](const std::string& size) {benchPCoeffLayerElementStats<3>(std::stoi(size)); }},
	{"pcoeffLayerElementStats4", [](const std::string& size) {benchPCoeffLayerElementStats<4>(std::stoi(size)); }},
	{"pcoeffLayerElementStats5", [](const std::string& size) {benchPCoeffLayerElementStats<5>(std::stoi(size)); }},
	{"pcoeffLayerElementStats6", [](const std::string& size) {benchPCoeffLayerElementStats<6>(std::stoi(size)); }},
	{"pcoeffLayerElementStats7", [](const std::string& size) {benchPCoeffLayerElementStats<7>(std::stoi(size)); }},

	{"pipelineTestSet1", [](const std::string& size) {pipelineTestSet<1>(std::stoi(size)); }},
	{"pipelineTestSet2", [](const std::string& size) {pipelineTestSet<2>(std::stoi(size)); }},
	{"pipelineTestSet3", [](const std::string& size) {pipelineTestSet<3>(std::stoi(size)); }},
	{"pipelineTestSet4", [](const std::string& size) {pipelineTestSet<4>(std::stoi(size)); }},
	{"pipelineTestSet5", [](const std::string& size) {pipelineTestSet<5>(std::stoi(size)); }},
	{"pipelineTestSet6", [](const std::string& size) {pipelineTestSet<6>(std::stoi(size)); }},
	{"pipelineTestSet7", [](const std::string& size) {pipelineTestSet<7>(std::stoi(size)); }},

	{"pipeline24PackTestSet", [](const std::string& size) {pipeline24PackTestSet(std::stoi(size)); }},
}};