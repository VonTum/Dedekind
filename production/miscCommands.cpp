#include "commands.h"

#include <iostream>
#include <fstream>
#include <random>
#include <map>
#include <string>
#include <chrono>
#include <algorithm>

#include "../dedelib/funcTypes.h"
#include "../dedelib/dedekindDecomposition.h"
#include "../dedelib/valuedDecomposition.h"
#include "../dedelib/toString.h"
#include "../dedelib/serialization.h"
#include "../dedelib/generators.h"

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

#include "../dedelib/pcoeffClasses.h"
#include "../dedelib/flatBufferManagement.h"

#include "../dedelib/randomMBFGeneration.h"
#include "../dedelib/dedekindEstimation.h"
#include "../dedelib/mbfFilter.h"

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
		for(size_t itemInLayer = 0; itemInLayer < layerSizes[Variables][layer]; itemInLayer++) {
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
		intervalStatsFile << ", " << layerSizes[Variables][layer];
	}
	intervalStatsFile << ", totalCount\n";
	intervalStatsFile.close();*/
	std::default_random_engine generator;
	for(size_t layer = 49; layer < (1 << Variables); layer++) {
		std::uniform_int_distribution<unsigned int> random(0, layerSizes[Variables][layer] - 1);
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

void countValidPermutationSetFraction(std::vector<size_t> counts, unsigned int groupVarIndex) {
	constexpr unsigned int Variables = 7;
	std::vector<TopBots<Variables>> topBots = readTopBots<Variables>(500);

	auto seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
	std::default_random_engine generator(seed);
	std::cout << "Seed: " << seed << std::endl;

	for(size_t topI = 0; topI < counts.size(); topI++) {
		size_t count = counts[topI];

		const TopBots<Variables>& selectedTop = topBots[std::uniform_int_distribution<size_t>(0, topBots.size() - 1)(generator)];
		std::uniform_int_distribution<size_t> botSelector(0, selectedTop.bots.size() - 1);

		Monotonic<Variables> top = selectedTop.top;
		permuteRandom(top, generator);

		std::cout << "Starting generation" << std::endl;

		size_t totalValidPermutationsTotal = 0;
		size_t totalPermutationsTestedTotal = 0;
		size_t validPermutationSetsTotal = 0;
		size_t totalPermutationSetsTotal = 0;

		for(size_t i = 0; i < count; i++) {
			Monotonic<Variables> bot = selectedTop.bots[botSelector(generator)];
			permuteRandom(bot, generator);

			size_t totalValidPermutations = 0;
			size_t validPermutationSets = 0;
			size_t totalPermutationSets = 0;
			
			bool permutationsThatAreValid[factorial(Variables)];
			size_t permutationValidIndex = 0;

			bot.forEachPermutation([&](const Monotonic<Variables>& permutedBot){
				permutationsThatAreValid[permutationValidIndex++] = (permutedBot <= selectedTop.top);
			});

			assert(permutationValidIndex == factorial(Variables));

			for(size_t i = 0; i < factorial(Variables); i += factorial(groupVarIndex)) {
				totalPermutationSets++;
				size_t numberValidInThisGroup = 0;
				for(size_t j = 0; j < factorial(groupVarIndex); j++) {
					totalPermutationsTestedTotal++;
					if(permutationsThatAreValid[i + j]) {
						numberValidInThisGroup++;
						totalValidPermutations++;
					}
				}
				if(numberValidInThisGroup != 0) {
					validPermutationSets++;
				}
			}

			
			// Has at least one valid permutation. This is necessary because these are already filitered out by the FlatMBFStructure
			if(validPermutationSets > 0) {
				validPermutationSetsTotal += validPermutationSets;
				totalPermutationSetsTotal += totalPermutationSets;
				totalValidPermutationsTotal += totalValidPermutations;
				
				//std::cout << "Permutation set found: " << validPermutationSets << "/" << totalPermutationSets << std::endl;
			}
		}
		std::cout << "Total fraction: " << validPermutationSetsTotal << "/" << totalPermutationSetsTotal << "=" << (double(validPermutationSetsTotal) / totalPermutationSetsTotal) << std::endl;
		std::cout << "Total valid permutations per valid set: " << totalValidPermutationsTotal << "/" << validPermutationSetsTotal << "=" << (double(totalValidPermutationsTotal) / validPermutationSetsTotal) << std::endl;
		std::cout << "Total permutation fraction:" << totalValidPermutationsTotal << "/" << totalPermutationsTestedTotal << "=" << (double(totalValidPermutationsTotal) / totalPermutationsTestedTotal) << std::endl;
	}
}

void printWindowStats(const std::vector<long long>& values, size_t windowSize) {
	long long valuesWindowSum = 0;
	for(size_t i = 0; i < windowSize; i++) {
		valuesWindowSum += values[i];
	}

	long long minValuesWindowSum = valuesWindowSum;
	long long maxValuesWindowSum = valuesWindowSum;
	for(size_t i = windowSize; i < values.size(); i++) {
		valuesWindowSum += values[i] - values[i-windowSize];
		minValuesWindowSum = std::min(minValuesWindowSum, valuesWindowSum);
		maxValuesWindowSum = std::max(maxValuesWindowSum, valuesWindowSum);
	}

	double differencePercentage = 100.0 * maxValuesWindowSum / minValuesWindowSum;
	std::cout << "For window size " << windowSize << " window sum range: " << minValuesWindowSum << "-" << maxValuesWindowSum << ": " << differencePercentage << "%" << std::endl;
	
}

void printStats(const std::vector<long long>& values) {
	long long totalSum = 0;
	for(size_t i = 0; i < values.size(); i++) {
		totalSum += values[i];
	}
	std::cout << "Total sum: " << totalSum << std::endl;
	printWindowStats(values, 5);
	printWindowStats(values, 15);
	printWindowStats(values, 50);
	printWindowStats(values, 150);
}

void printBasicMixInfo(const std::vector<long long>& oldValues, size_t divisions) {
	std::vector<long long> reshuffledValues;
	reshuffledValues.reserve(oldValues.size());

	size_t CHUNK_OFFSET = oldValues.size() / divisions;
	for(size_t i = 0; i < oldValues.size(); i++) {
		size_t mod = i % divisions;
		size_t set = i / divisions;
		size_t source = CHUNK_OFFSET * mod + set;
		reshuffledValues.push_back(oldValues[source]);
	}

	std::cout << "Stats for shuffle with " << divisions << " divisions:" << std::endl;
	printStats(reshuffledValues);
}

template<typename T>
T bitReverse(T original, size_t bits) {
	T result = 0;
	for(size_t i = 0; i < (bits+1) / 2; i++) {
		result |= (original & (static_cast<T>(1) << i)) << (bits - 2 * i - 1);
		result |= (original & (static_cast<T>(1) << (bits - i - 1))) >> (bits - 2 * i - 1);
	}
	return result;
}

void printReversingMixInfo(const std::vector<long long>& oldValues, size_t divisionsLOG2) {
	size_t divisions = 1 << divisionsLOG2;
	std::vector<long long> reshuffledValues;
	reshuffledValues.reserve(oldValues.size());

	size_t CHUNK_OFFSET = oldValues.size() / divisions;
	for(size_t i = 0; i < oldValues.size(); i++) {
		size_t mod = i % divisions;
		size_t set = i / divisions;
		size_t source = CHUNK_OFFSET * bitReverse(mod, divisionsLOG2) + set;
		reshuffledValues.push_back(oldValues[source]);
	}

	std::cout << "Stats for index reversing shuffle with " << divisions << " divisions:" << std::endl;
	printStats(reshuffledValues);
}

void testMixingPattern() {
	constexpr size_t SIZE = 910*1024;
	
	std::vector<long long> expensivenessValues;
	expensivenessValues.reserve(SIZE);
	
	// Generate a model difficulty distribution
	for(size_t i = 0; i < SIZE; i++) {
		double x = double(i) / SIZE;
		double estimatedExpensiveness = std::exp(12*x)*(1-x); // About matches the real distribution
		expensivenessValues.push_back(static_cast<long long>(estimatedExpensiveness*1000));
	}

	std::cout << "Stats for baseline:" << std::endl;
	printStats(expensivenessValues);

	printBasicMixInfo(expensivenessValues, 4);
	printBasicMixInfo(expensivenessValues, 8);
	printBasicMixInfo(expensivenessValues, 16);
	printBasicMixInfo(expensivenessValues, 32);
	printBasicMixInfo(expensivenessValues, 64);
	printBasicMixInfo(expensivenessValues, 128);

	printReversingMixInfo(expensivenessValues, 2);
	printReversingMixInfo(expensivenessValues, 3);
	printReversingMixInfo(expensivenessValues, 4);
	printReversingMixInfo(expensivenessValues, 5);
	printReversingMixInfo(expensivenessValues, 6);
	printReversingMixInfo(expensivenessValues, 7);
}


template<unsigned int Variables>
void computeUnateNumbers() {
	const Monotonic<Variables>* mbfs = readFlatBufferNoMMAP<Monotonic<Variables>>(FileName::flatMBFs(Variables), mbfCounts[Variables]);
	const ClassInfo* allBigIntervalSizes = readFlatBufferNoMMAP<ClassInfo>(FileName::flatClassInfo(Variables), mbfCounts[Variables]);

	uint64_t totalUnates = 0;
	uint64_t totalInequivalentUnates = 0;
	uint64_t totalUnatesPerSize[(1 << Variables) + 1]{0};
	uint64_t totalMBFsPerSize[(1 << Variables) + 1]{0};
	uint64_t totalInequivalentUnatesPerSize[(1 << Variables) + 1]{0}; // == totalInequivalentMBFsPerSize

	for(size_t i = 0; i < mbfCounts[Variables]; i++) {
		uint64_t nonSymmetricVariables = 0;
		BooleanFunction<Variables> bf = mbfs[i].bf;
		for(unsigned int v = 0; v < Variables; v++) {
			BooleanFunction<Variables> copy = bf;
			copy.invertVariable(v);
			if(copy != bf) {
				nonSymmetricVariables++;
			}
		}
		
		uint64_t unateCount = allBigIntervalSizes[i].classSize * (1 << nonSymmetricVariables);

		totalUnates += unateCount;
		totalInequivalentUnates += (1 << nonSymmetricVariables);
		size_t sz = mbfs[i].bf.size();
		/*if(sz <= (1 << Variables) / 2) {
			totalInequivalentUnates++;
		}*/
		totalUnatesPerSize[sz] += unateCount;
		totalInequivalentUnatesPerSize[sz] += (1 << nonSymmetricVariables);
		totalMBFsPerSize[sz] += allBigIntervalSizes[i].classSize;
	}

	std::cout << "Size\t\t\tUnates\t\t\tMBFs\t\t\tEq Classes" << std::endl;
	for(size_t sz = 0; sz <= 1 << Variables; sz++) {
		std::cout << sz << ":\t\t\t" << totalUnatesPerSize[sz] << "\t\t\t" << totalMBFsPerSize[sz] << "\t\t\t" << totalInequivalentUnatesPerSize[sz] << std::endl;
	}
	std::cout << std::endl;
	std::cout << "Total Unates: " << totalUnates << std::endl;
	std::cout << "Total Inequivalent Unates: " << totalInequivalentUnates << std::endl;
}

template<unsigned int Variables>
void countMBFSizeStatistics() {
	size_t fileSize;
	const Monotonic<Variables>* mbfs = static_cast<const Monotonic<Variables>*>(mmapWholeFileSequentialRead(FileName::randomMBFs(Variables), fileSize));

	size_t numMBFs = fileSize / sizeof(Monotonic<Variables>);

	if(numMBFs > 1000*1000*1000) {numMBFs = 1000*1000*1000;}

	int sizeDistribution[(1 << Variables) + 1];
	for(size_t i = 0; i < (1 << Variables) + 1; i++) {
		sizeDistribution[i] = 0;
	}


	for(size_t i = 0; i < numMBFs; i++) {
		size_t ones = mbfs[i].size();
		sizeDistribution[ones]++;
	}

	std::cout << "In LATEX pgfplots format" << std::endl;
	std::cout << "Counted " << numMBFs << " MBFs:" << std::endl;
	for(size_t i = 0; i < (1 << Variables) + 1; i++) {
		std::cout << "(" << i << "," << sizeDistribution[i] << ")" << std::endl;
	}
}

CommandSet miscCommands{"Misc", {
	{"ramTest", []() {doRAMTest(); }},
	{"testMixingPattern", [](){testMixingPattern();}},

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

	{"computeUnateNumbers1", computeUnateNumbers<1>},
	{"computeUnateNumbers2", computeUnateNumbers<2>},
	{"computeUnateNumbers3", computeUnateNumbers<3>},
	{"computeUnateNumbers4", computeUnateNumbers<4>},
	{"computeUnateNumbers5", computeUnateNumbers<5>},
	{"computeUnateNumbers6", computeUnateNumbers<6>},
	{"computeUnateNumbers7", computeUnateNumbers<7>},

	{"benchmarkRandomMBFGeneration1", benchmarkRandomMBFGeneration<1>},
	{"benchmarkRandomMBFGeneration2", benchmarkRandomMBFGeneration<2>},
	{"benchmarkRandomMBFGeneration3", benchmarkRandomMBFGeneration<3>},
	{"benchmarkRandomMBFGeneration4", benchmarkRandomMBFGeneration<4>},
	{"benchmarkRandomMBFGeneration5", benchmarkRandomMBFGeneration<5>},
	{"benchmarkRandomMBFGeneration6", benchmarkRandomMBFGeneration<6>},
	{"benchmarkRandomMBFGeneration7", benchmarkRandomMBFGeneration<7>},

	{"benchmarkMBF9Generation", benchmarkMBF9Generation},

	{"naiveD10Estimation", naiveD10Estimation},

	{"countValidPermutationSetFraction7_0", []() {countValidPermutationSetFraction(std::vector<size_t>{10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000}, 0); }},
	{"countValidPermutationSetFraction7_1", []() {countValidPermutationSetFraction(std::vector<size_t>{10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000}, 1); }},
	{"countValidPermutationSetFraction7_2", []() {countValidPermutationSetFraction(std::vector<size_t>{10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000}, 2); }},
	{"countValidPermutationSetFraction7_3", []() {countValidPermutationSetFraction(std::vector<size_t>{10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000}, 3); }},
	{"countValidPermutationSetFraction7_4", []() {countValidPermutationSetFraction(std::vector<size_t>{10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000}, 4); }},
	{"countValidPermutationSetFraction7_5", []() {countValidPermutationSetFraction(std::vector<size_t>{10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000}, 5); }},
	{"countValidPermutationSetFraction7_6", []() {countValidPermutationSetFraction(std::vector<size_t>{10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000}, 6); }},
	{"countValidPermutationSetFraction7_7", []() {countValidPermutationSetFraction(std::vector<size_t>{10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000}, 7); }},

	{"estimateNextDedekindNumber1", estimateNextDedekindNumber<1>},
	{"estimateNextDedekindNumber2", estimateNextDedekindNumber<2>},
	{"estimateNextDedekindNumber3", estimateNextDedekindNumber<3>},
	{"estimateNextDedekindNumber4", estimateNextDedekindNumber<4>},
	{"estimateNextDedekindNumber5", estimateNextDedekindNumber<5>},
	{"estimateNextDedekindNumber6", estimateNextDedekindNumber<6>},
	{"estimateNextDedekindNumber7", estimateNextDedekindNumber<7>},
	{"estimateNextDedekindNumber8", estimateNextDedekindNumber<8>},
	{"estimateNextDedekindNumber9", estimateNextDedekindNumber<9>},

	{"codeGenGetSignature", codeGenGetSignature},

	{"makeSignatureStatistics1", makeSignatureStatistics<1>},
	{"makeSignatureStatistics2", makeSignatureStatistics<2>},
	{"makeSignatureStatistics3", makeSignatureStatistics<3>},
	{"makeSignatureStatistics4", makeSignatureStatistics<4>},
	{"makeSignatureStatistics5", makeSignatureStatistics<5>},
	{"makeSignatureStatistics6", makeSignatureStatistics<6>},
	{"makeSignatureStatistics7", makeSignatureStatistics<7>},
	{"makeSignatureStatistics8", makeSignatureStatistics<8>},
	{"makeSignatureStatistics9", makeSignatureStatistics<9>},

	{"testFilterTreePerformance1", testFilterTreePerformance<1>},
	{"testFilterTreePerformance2", testFilterTreePerformance<2>},
	{"testFilterTreePerformance3", testFilterTreePerformance<3>},
	{"testFilterTreePerformance4", testFilterTreePerformance<4>},
	{"testFilterTreePerformance5", testFilterTreePerformance<5>},
	{"testFilterTreePerformance6", testFilterTreePerformance<6>},
	{"testFilterTreePerformance7", testFilterTreePerformance<7>},
	{"testFilterTreePerformance8", testFilterTreePerformance<8>},
	{"testFilterTreePerformance9", testFilterTreePerformance<9>},

	{"testTreeLessFilterTreePerformance1", testTreeLessFilterTreePerformance<1>},
	{"testTreeLessFilterTreePerformance2", testTreeLessFilterTreePerformance<2>},
	{"testTreeLessFilterTreePerformance3", testTreeLessFilterTreePerformance<3>},
	{"testTreeLessFilterTreePerformance4", testTreeLessFilterTreePerformance<4>},
	{"testTreeLessFilterTreePerformance5", testTreeLessFilterTreePerformance<5>},
	{"testTreeLessFilterTreePerformance6", testTreeLessFilterTreePerformance<6>},
	{"testTreeLessFilterTreePerformance7", testTreeLessFilterTreePerformance<7>},
	{"testTreeLessFilterTreePerformance8", testTreeLessFilterTreePerformance<8>},
	{"testTreeLessFilterTreePerformance9", testTreeLessFilterTreePerformance<9>},
	
	{"countMBFSizeStatistics1", countMBFSizeStatistics<1>},
	{"countMBFSizeStatistics2", countMBFSizeStatistics<2>},
	{"countMBFSizeStatistics3", countMBFSizeStatistics<3>},
	{"countMBFSizeStatistics4", countMBFSizeStatistics<4>},
	{"countMBFSizeStatistics5", countMBFSizeStatistics<5>},
	{"countMBFSizeStatistics6", countMBFSizeStatistics<6>},
	{"countMBFSizeStatistics7", countMBFSizeStatistics<7>},
	{"countMBFSizeStatistics8", countMBFSizeStatistics<8>},
	{"countMBFSizeStatistics9", countMBFSizeStatistics<9>},
}, {
	{"checkIntervalLayers1", [](const std::vector<std::string>& size) {checkIntervalLayers<1>(std::stoi(size[0])); }},
	{"checkIntervalLayers2", [](const std::vector<std::string>& size) {checkIntervalLayers<2>(std::stoi(size[0])); }},
	{"checkIntervalLayers3", [](const std::vector<std::string>& size) {checkIntervalLayers<3>(std::stoi(size[0])); }},
	{"checkIntervalLayers4", [](const std::vector<std::string>& size) {checkIntervalLayers<4>(std::stoi(size[0])); }},
	{"checkIntervalLayers5", [](const std::vector<std::string>& size) {checkIntervalLayers<5>(std::stoi(size[0])); }},
	{"checkIntervalLayers6", [](const std::vector<std::string>& size) {checkIntervalLayers<6>(std::stoi(size[0])); }},
	{"checkIntervalLayers7", [](const std::vector<std::string>& size) {checkIntervalLayers<7>(std::stoi(size[0])); }},
	//{"checkIntervalLayers8", [](const std::vector<std::string>& size) {checkIntervalLayers<8>(std::stoi(size[0])); }},
	//{"checkIntervalLayers9", [](const std::vector<std::string>& size) {checkIntervalLayers<9>(std::stoi(size[0])); }}

	{"pcoeffLayerElementStats1", [](const std::vector<std::string>& size) {benchPCoeffLayerElementStats<1>(std::stoi(size[0])); }},
	{"pcoeffLayerElementStats2", [](const std::vector<std::string>& size) {benchPCoeffLayerElementStats<2>(std::stoi(size[0])); }},
	{"pcoeffLayerElementStats3", [](const std::vector<std::string>& size) {benchPCoeffLayerElementStats<3>(std::stoi(size[0])); }},
	{"pcoeffLayerElementStats4", [](const std::vector<std::string>& size) {benchPCoeffLayerElementStats<4>(std::stoi(size[0])); }},
	{"pcoeffLayerElementStats5", [](const std::vector<std::string>& size) {benchPCoeffLayerElementStats<5>(std::stoi(size[0])); }},
	{"pcoeffLayerElementStats6", [](const std::vector<std::string>& size) {benchPCoeffLayerElementStats<6>(std::stoi(size[0])); }},
	{"pcoeffLayerElementStats7", [](const std::vector<std::string>& size) {benchPCoeffLayerElementStats<7>(std::stoi(size[0])); }},

	{"estimateDPlusOneWithPairs1", estimateDPlusOneWithPairs<1>},
	{"estimateDPlusOneWithPairs2", estimateDPlusOneWithPairs<2>},
	{"estimateDPlusOneWithPairs3", estimateDPlusOneWithPairs<3>},
	{"estimateDPlusOneWithPairs4", estimateDPlusOneWithPairs<4>},
	{"estimateDPlusOneWithPairs5", estimateDPlusOneWithPairs<5>},
	{"estimateDPlusOneWithPairs6", estimateDPlusOneWithPairs<6>},
	{"estimateDPlusOneWithPairs7", estimateDPlusOneWithPairs<7>},
	{"estimateDPlusOneWithPairs8", estimateDPlusOneWithPairs<8>},
	{"estimateDPlusOneWithPairs9", estimateDPlusOneWithPairs<9>},

	{"estimateDedekind", [](const std::vector<std::string>& vars) {
		int n = std::stoi(vars[0]);

		for(int i = 0; i <= n; i++) {
			std::cout << "D(" << i << ") = " << estimateDedekindNumber(i) << std::endl;
		}
	}},

	{"parallelizeMBF9GenerationAcrossAllCores", [](const std::vector<std::string>& vars) {
		int n = std::stoi(vars[0]);

		parallelizeMBF9GenerationAcrossAllCores(n);
	}},
}};
