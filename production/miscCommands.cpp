#include "commands.h"

#include <iostream>
#include <fstream>
#include <random>
#include <map>
#include <string>
#include <chrono>
#include <algorithm>

#include <sys/mman.h>

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

#include <math.h>
static double a(int n) {
	int ch = choose(n, n / 2 - 1);
	double powers = pow(2.0, -n/2) + n*n*pow(2.0, -n-5) - n * pow(2.0, -n-4);
	return ch * powers;
}
static double b(int n) {
	int ch = choose(n, (n - 3) / 2);
	double powers = pow(2.0, -(n+3) / 2) - n*n*pow(2.0, -n-5) - n * pow(2.0, -n-3);
	return ch * powers;
}
static double c(int n) {
	int ch = choose(n, (n - 1) / 2);
	double powers = pow(2.0, -(n+1) / 2) + n*n*pow(2.0, -n-4);
	return ch * powers;
}
static double estimateDedekindNumber(int n) {
	if(n % 2 == 0) {
		return pow(2.0, choose(n, n / 2)) * exp(a(n));
	} else {
		return pow(2.0, choose(n, (n-1) / 2) + 1) * exp(b(n) + c(n));
	}
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

template<unsigned int Variables, size_t PREFETCH_CACHE_SIZE = 64>
struct MBFSampler{
	struct CumulativeBuffer{
		uint64_t mbfCountHere;
		uint64_t inverseClassSize; // factorial(Variables) / classSize // Used to get rid of a division
		Monotonic<Variables>* mbfs;
	};

	std::unique_ptr<CumulativeBuffer[]> cumulativeBuffers;
	Monotonic<Variables>* mbfsByClassSize;

	Monotonic<Variables>* prefetchCache[PREFETCH_CACHE_SIZE];
	size_t curPrefetchLine;

	MBFSampler() {
		constexpr int VAR_FACTORIAL = factorial(Variables);
		struct BufferStruct {
			int classSize;
			std::vector<Monotonic<Variables>> mbfs;
		};

		std::cout << "Reading buffers..." << std::endl;
		const Monotonic<Variables>* mbfs = readFlatBufferNoMMAP<Monotonic<Variables>>(FileName::flatMBFs(Variables), mbfCounts[Variables]);
		const ClassInfo* allBigIntervalSizes = readFlatBufferNoMMAP<ClassInfo>(FileName::flatClassInfo(Variables), mbfCounts[Variables]);
		
		std::cout << "Sorting buffers..." << std::endl;
		
		std::vector<BufferStruct> buffers;
		
		// Have sizes sorted in decending order, because most likely classes have full 5040 permutations!
		for(int i = VAR_FACTORIAL / 2; i > 0; i--) {
			if(VAR_FACTORIAL % i == 0) {
				BufferStruct newStruct{i, std::vector<Monotonic<Variables>>{}};
				buffers.push_back(std::move(newStruct));
			}
		}

		// Add all MBFs to these groups
		// Already push the biggest group into the final buffer, saves on a big allocation
		size_t allocSizeBytes = sizeof(Monotonic<Variables>) * mbfCounts[Variables];
		std::cout << "Allocating huge pages..." << std::endl;
		Monotonic<Variables>* mbfsByClassSize = (Monotonic<Variables>*) aligned_alloc(1024*1024*1024, allocSizeBytes);
		madvise(mbfsByClassSize, allocSizeBytes, MADV_HUGEPAGE);
		madvise(mbfsByClassSize, allocSizeBytes, MADV_WILLNEED);

		// MMAP to force 1GB pages gives segmentation fault
		//Monotonic<Variables>* mbfsByClassSize = (Monotonic<Variables>*) mmap(nullptr, allocSizeBytes, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE | MAP_HUGETLB | (30 << MAP_HUGE_SHIFT), -1, 0);
		if(mbfsByClassSize == nullptr) {
			perror("While allocating 1GB pages");
			exit(1);
		}
		Monotonic<Variables>* curMBFsPtr = mbfsByClassSize;
		for(size_t i = 0; i < mbfCounts[Variables]; i++) {
			uint64_t classSize = allBigIntervalSizes[i].classSize;
			if(classSize == VAR_FACTORIAL) {
				*curMBFsPtr++ = mbfs[i];
			} else {
				for(BufferStruct& bf : buffers) {
					if(bf.classSize == classSize) {
						bf.mbfs.push_back(mbfs[i]);
						break;
					}
				}
			}
		}

		std::cout << "Regrouping buffers..." << std::endl;
		freeFlatBufferNoMMAP(mbfs, mbfCounts[Variables]); // Free up memory
		freeFlatBufferNoMMAP(allBigIntervalSizes, mbfCounts[Variables]); // Free up memory
		// Now move them all to one large buffer (not really necessary but makes everything a little neater)
		std::unique_ptr<CumulativeBuffer[]> cumulativeBuffers = std::unique_ptr<CumulativeBuffer[]>(new CumulativeBuffer[1 + buffers.size()]);
		cumulativeBuffers[0] = CumulativeBuffer{(curMBFsPtr - mbfsByClassSize) * VAR_FACTORIAL, 1, mbfsByClassSize};
		for(size_t i = 0; i < buffers.size(); i++) {
			BufferStruct& bf = buffers[i];
			uint64_t mbfCountHere = bf.classSize * bf.mbfs.size();
			cumulativeBuffers[i+1] = CumulativeBuffer{mbfCountHere, VAR_FACTORIAL / bf.classSize, curMBFsPtr};
			for(Monotonic<Variables>& mbf : bf.mbfs) {
				*curMBFsPtr++ = mbf;
			}
		}

		this->mbfsByClassSize = std::move(mbfsByClassSize);
		this->cumulativeBuffers = std::move(cumulativeBuffers);

		for(size_t i = 0; i < PREFETCH_CACHE_SIZE; i++) {
			this->prefetchCache[i] = this->mbfsByClassSize;
		}
		this->curPrefetchLine = 0;
		// Get some samples to initialize prefetchCache with good random samples
		std::default_random_engine tmpGenerator;
		for(size_t i = 0; i < PREFETCH_CACHE_SIZE; i++) {
			this->sample(tmpGenerator);
		}
	}

	Monotonic<Variables> sample(std::default_random_engine& generator) {
		constexpr int VAR_FACTORIAL = factorial(Variables);
		
		uint64_t chosenMBFIdx = std::uniform_int_distribution<uint64_t>(0, dedekindNumbers[Variables] - 1)(generator);
		
		CumulativeBuffer* cb = cumulativeBuffers.get();
		while(true) { // Cheap iteration through data in L1 cache
			if(chosenMBFIdx < cb->mbfCountHere) { // will nearly always hit first cumulative buffer, because it's the largest and has the largest factor
				size_t idxInBuffer = chosenMBFIdx * cb->inverseClassSize / VAR_FACTORIAL; // Division by constant is cheap!

				Monotonic<Variables>* foundMBF = cb->mbfs + idxInBuffer;// Expensive Main Memory access, so we prefetch it and return a result that has already arrived
				_mm_prefetch((char const *) foundMBF, _MM_HINT_T0);
				Monotonic<Variables> thisResult = *this->prefetchCache[this->curPrefetchLine];
				this->prefetchCache[this->curPrefetchLine] = foundMBF;
				this->curPrefetchLine = (this->curPrefetchLine + 1) % PREFETCH_CACHE_SIZE;
				return thisResult;
			} else {
				chosenMBFIdx -= cb->mbfCountHere;
			}
			cb++;
		}
	}
};

template<unsigned int Variables>
struct FastRandomPermuter {
	FastRandomPermuter() {};
	BooleanFunction<Variables> permuteRandom(BooleanFunction<7> bf, std::default_random_engine& generator) const {
		permuteRandom(bf, generator);
		return bf;
	}
};

 /*alignas(64) struct FastRandomPermuter<7> {
	alignas(64) struct PrecomputedPermuteInfo {
		__m128i masks65; // 64 bits each. _mm_shuffle_epi32 and _mm_broadcastd_epi64 to broadcast
		__m128i masks4321; // 32 bits each. _mm_shuffle_epi32 and _mm_broadcastd_epi32 to broadcast
		__m128i shifts0654; // 32 bits each, elem 3 is 0. _mm_shuffle_epi32 to broadcast
		__m128i shifts0321; // 32 bits each, elem 3 is 0. _mm_shuffle_epi32 to broadcast
	};

	// The first 720 elements don't swap var 7, so we simply detect this and copy mask 5
	PrecomputedPermuteInfo infos[5040];

	FastRandomPermuter() {
		PrecomputedPermuteInfo* curInfo = this->infos;
		for(int v6 = 6; v6 >= 0; v6--) { // Do annoying case first where we don't swap var 6. 
			// This first mask is special, because it shifts across the 64 bit boundary. 
			// If there should be no swap (6 with 6), in this case just store 0
			// Else just store the mask and shift for v6 on its own. 
			uint64_t combined_mask6 = v6 != 6 ? BooleanFunction<6>::varMask(v6).data : uint64_t(0);
			uint32_t shift6 = (1 << 6) - (1 << v6);
			for(int v5 = 0; v5 < 6; v5++) {
				uint64_t combined_mask5 = andnot(BooleanFunction<6>::varMask(v5), BooleanFunction<6>::varMask(5)).data;
				uint32_t shift5 = (1 << 5) - (1 << v5);
				for(int v4 = 0; v4 < 5; v4++) {
					uint32_t combined_mask4 = andnot(BooleanFunction<5>::varMask(v4), BooleanFunction<5>::varMask(4)).data;
					uint32_t shift4 = (1 << 4) - (1 << v4);
					for(int v3 = 0; v3 < 4; v3++) {
						uint32_t combined_mask3 = andnot(BooleanFunction<5>::varMask(v3), BooleanFunction<5>::varMask(3)).data;
						uint32_t shift3 = (1 << 3) - (1 << v3);
						for(int v2 = 0; v2 < 3; v2++) {
							uint32_t combined_mask2 = andnot(BooleanFunction<5>::varMask(v2), BooleanFunction<5>::varMask(2)).data;
							uint32_t shift2 = (1 << 2) - (1 << v2);
							for(int v1 = 0; v1 < 2; v1++) {
								uint32_t combined_mask1 = andnot(BooleanFunction<5>::varMask(v1), BooleanFunction<5>::varMask(1)).data;
								uint32_t shift1 = (1 << 1) - (1 << v1);

								curInfo->shifts0654 = _mm_set_epi32(0, shift6, shift5, shift4);
								curInfo->shifts0321 = _mm_set_epi32(0, shift3, shift2, shift1);
								curInfo->masks65 = _mm_set_epi64x(combined_mask6, combined_mask5);
								curInfo->masks4321 = _mm_set_epi32(combined_mask4, combined_mask3, combined_mask2, combined_mask1);
								curInfo++;
							}
						}	
					}	
				}
			}
		}
	}
	__m128i permuteWithIndex(size_t chosenPermutation, __m128i bf) const {
		const PrecomputedPermuteInfo& info = this->infos[chosenPermutation];

		__m128i masks65 = _mm_load_si128(&info.masks65);
		__m128i shifts0654 = _mm_load_si128(&info.shifts0654);
		if(chosenPermutation >= 720) { // Do swap var 6
			// TODO
		}

		// Swap 5
		__m128i leftMask5 = _mm_broadcastq_epi64(masks65);
		__m128i shift5 = _mm_shuffle_epi32(shifts0654, _MM_SHUFFLE(3,1,3,1));
		
		__m128i rightMask5 = _mm_srl_epi64(leftMask5, shift5);
		__m128i bitsThatAreMovedMask = _mm_or_si128(leftMask5, rightMask5);

		__m128i leftBitsMovedRight = 

		__m128i masks4321 = _mm_load_si128(&info.masks4321);
		__m128i shifts0321 = _mm_load_si128(&info.shifts0321);

	}
	BooleanFunction<7> permuteRandom(BooleanFunction<7> bf, std::default_random_engine& generator) const {
		size_t selectedIndex = std::uniform_int_distribution<size_t>(0, 5039)(generator);
		bf.bitset.data = this->permuteWithIndex(selectedIndex, bf.bitset.data);
		return bf;
	}
};*/

template<> alignas(64) struct FastRandomPermuter<7> {
	__m128i permute3456[24];

	struct PrecomputedPermuteInfo {
		__m128i shiftAndSwapMaskX; // stores 0, shift1, shift0, mask0
		__m128i shiftAndSwapMaskY; // stores 0, shift2, mask2, mask1
		__m128i shiftLeft0;
		__m128i shiftRight0;
		__m128i shiftLeft1;
		__m128i shiftRight1;
		__m128i shiftLeft2;
		__m128i shiftRight2;
	};

	PrecomputedPermuteInfo bigPermuteInfo[210];

	FastRandomPermuter() {
		// Initialize permute4567 masks
		__m128i identy = _mm_set_epi8(15,14,13,12,11,10,9 ,8 ,7 ,6 ,5 ,4 ,3 ,2 ,1 ,0);
		__m128i swap56 = _mm_set_epi8(15,14,13,12,7 ,6 ,5 ,4 ,11,10,9 ,8 ,3 ,2 ,1 ,0);
		__m128i swap46 = _mm_set_epi8(15,14,7 ,6 ,11,10,3 ,2 ,13,12,5 ,4 ,9 ,8 ,1 ,0);
		__m128i swap36 = _mm_set_epi8(15,7 ,13,5 ,11,3 ,9 ,1 ,14,6 ,12,4 ,10,2 ,8 ,0);

		// All permutations of last three
		__m128i p3456 = identy;
		__m128i p3465 = swap56;
		__m128i p3654 = swap46;
		__m128i p3645 = _mm_shuffle_epi8(p3654, swap56);
		__m128i p3546 = _mm_shuffle_epi8(p3645, swap46);
		__m128i p3564 = _mm_shuffle_epi8(p3546, swap56);

		__m128i permutesLastThree[6]{p3456, p3465, p3654, p3645, p3546, p3564};
		__m128i toPermute[4]{identy, swap36, _mm_shuffle_epi8(p3465, swap36), _mm_shuffle_epi8(p3564, swap36)};

		{
			size_t i = 0;
			for(__m128i to : toPermute) {
				for(__m128i permut : permutesLastThree) {
					this->permute3456[i++] = _mm_shuffle_epi8(to, permut);
				}
			}
		}


		size_t i = 0;
		// Remaining 7, 6, 5 permutations
		for(unsigned int v0 = 0; v0 < 7; v0++) {
			PrecomputedPermuteInfo permutInfo;


			//                v0 = x x _ _ x x _ _
			//                 0 = x _ x _ x _ x _
			//  shiftLeft0 = _ _ x _ _ _ x _ = 0 & ~v0
			// shiftRight0 = _ x _ _ _ x _ _ = v0 & ~0 
			// shift = 1, swap = 0

			// 64 bit crossing case
			//                v0 = x x x x _ _ _ _
			//                 0 = x _ x _ x _ x _
			// shiftRight0 = _ x _ x _ _ _ _ = 0 & ~v0 // Reverse the variables
			//  shiftLeft0 = _ _ _ _ x _ x _ = v0 & ~0 
			// shift = 1, swap = 1

			uint32_t shift0 = (1 << v0) - (1 << 0);
			uint32_t shouldSwap0 = 0x00000000;
			permutInfo.shiftLeft0 = andnot(BooleanFunction<7>::varMask(0), BooleanFunction<7>::varMask(v0)).data;
			permutInfo.shiftRight0 = andnot(BooleanFunction<7>::varMask(v0), BooleanFunction<7>::varMask(0)).data;
			if(v0 == 6) {
				shift0 = (1 << 0);
				shouldSwap0 = 0xFFFFFFFF;
				__m128i tmp = permutInfo.shiftLeft0;
				permutInfo.shiftLeft0 = permutInfo.shiftRight0;
				permutInfo.shiftRight0 = tmp;
			}
			for(unsigned int v1 = 1; v1 < 7; v1++) {
				uint32_t shift1 = (1 << v1) - (1 << 1);
				uint32_t shouldSwap1 = 0x00000000;
				permutInfo.shiftLeft1 = andnot(BooleanFunction<7>::varMask(1), BooleanFunction<7>::varMask(v1)).data;
				permutInfo.shiftRight1 = andnot(BooleanFunction<7>::varMask(v1), BooleanFunction<7>::varMask(1)).data;
				if(v1 == 6) {
					shift1 = (1 << 1);
					shouldSwap1 = 0xFFFFFFFF;
					__m128i tmp = permutInfo.shiftLeft1;
					permutInfo.shiftLeft1 = permutInfo.shiftRight1;
					permutInfo.shiftRight1 = tmp;
				}
				for(unsigned int v2 = 2; v2 < 7; v2++) {
					uint32_t shift2 = (1 << v2) - (1 << 2);
					uint32_t shouldSwap2 = 0x00000000;
					permutInfo.shiftLeft2 = andnot(BooleanFunction<7>::varMask(2), BooleanFunction<7>::varMask(v2)).data;
					permutInfo.shiftRight2 = andnot(BooleanFunction<7>::varMask(v2), BooleanFunction<7>::varMask(2)).data;
					if(v2 == 6) {
						shift2 = (1 << 2);
						shouldSwap2 = 0xFFFFFFFF;
						__m128i tmp = permutInfo.shiftLeft2;
						permutInfo.shiftLeft2 = permutInfo.shiftRight2;
						permutInfo.shiftRight2 = tmp;
					}
					
					permutInfo.shiftAndSwapMaskX = _mm_set_epi32(0, shift1, shift0, shouldSwap0);
					permutInfo.shiftAndSwapMaskY = _mm_set_epi32(0, shift2, shouldSwap2, shouldSwap1);

					this->bigPermuteInfo[i++] = permutInfo;
				}
			}
		}
	}
	inline __m128i applyPreComputedSwap(__m128i bf, __m128i shiftLeft, __m128i shiftRight, __m128i shift, __m128i shouldSwap) const {
		__m128i bitsToShiftLeft = _mm_and_si128(bf, shiftLeft);
		__m128i bitsToShiftRight = _mm_and_si128(bf, shiftRight);
		__m128i bitsToKeep = _mm_andnot_si128(_mm_or_si128(shiftLeft, shiftRight), bf);
		__m128i combinedShiftedBits = _mm_or_si128(_mm_sll_epi64(bitsToShiftLeft, shift), _mm_srl_epi64(bitsToShiftRight, shift));
		__m128i swappedCombinedShiftedBits = _mm_shuffle_epi32(combinedShiftedBits, _MM_SHUFFLE(1, 0, 3, 2));
		__m128i shiftedAndSwapped = _mm_blendv_epi8(combinedShiftedBits, swappedCombinedShiftedBits, shouldSwap);
		return _mm_or_si128(shiftedAndSwapped, bitsToKeep);
	}
	__m128i permuteWithIndex(uint16_t chosenPermutation, __m128i bf) const {
		uint16_t permut3456 = chosenPermutation % 24;
		chosenPermutation /= 24;
		
		const PrecomputedPermuteInfo& bigPermut = this->bigPermuteInfo[chosenPermutation];
		__m128i shiftAndSwapMaskX = _mm_load_si128(&bigPermut.shiftAndSwapMaskX); // stores 0, shift1, shift0, mask0
		__m128i shiftAndSwapMaskY = _mm_load_si128(&bigPermut.shiftAndSwapMaskY); // stores 0, shift2, mask2, mask1

		__m128i shift0 = _mm_shuffle_epi32(shiftAndSwapMaskX, _MM_SHUFFLE(3, 1, 3, 1));
		__m128i shouldSwap0 = _mm_broadcastd_epi32(shiftAndSwapMaskX);
		bf = this->applyPreComputedSwap(bf, _mm_load_si128(&bigPermut.shiftLeft0), _mm_load_si128(&bigPermut.shiftRight0), shift0, shouldSwap0);

		__m128i shift1 = _mm_shuffle_epi32(shiftAndSwapMaskX, _MM_SHUFFLE(3, 2, 3, 2));
		__m128i shouldSwap1 = _mm_broadcastd_epi32(shiftAndSwapMaskY);
		bf = this->applyPreComputedSwap(bf, _mm_load_si128(&bigPermut.shiftLeft1), _mm_load_si128(&bigPermut.shiftRight1), shift1, shouldSwap1);

		__m128i shift2 = _mm_shuffle_epi32(shiftAndSwapMaskY, _MM_SHUFFLE(3, 2, 3, 2));
		__m128i shouldSwap2 = _mm_shuffle_epi32(shiftAndSwapMaskY, _MM_SHUFFLE(1, 1, 1, 1));
		bf = this->applyPreComputedSwap(bf, _mm_load_si128(&bigPermut.shiftLeft2), _mm_load_si128(&bigPermut.shiftRight2), shift2, shouldSwap2);

		bf = _mm_shuffle_epi8(bf, this->permute3456[permut3456]);
		return bf;
	}
	BooleanFunction<7> permuteRandom(BooleanFunction<7> bf, std::default_random_engine& generator) const {
		size_t selectedIndex = std::uniform_int_distribution<size_t>(0, 5039)(generator);
		bf.bitset.data = this->permuteWithIndex(selectedIndex, bf.bitset.data);
		return bf;
	}
};

template<unsigned int Variables>
void testFastRandomPermuter(BooleanFunction<Variables> sample5040) {
	constexpr unsigned int VAR_FACTORIAL = factorial(Variables);

	BooleanFunction<Variables> sampleCopy = sample5040;

	std::cout << "Testing FastRandomPermuter..." << std::endl;
	FastRandomPermuter<Variables> permuter;

	BooleanFunction<Variables> table[VAR_FACTORIAL];
	{
		size_t i = 0;
		sample5040.forEachPermutation([&](BooleanFunction<Variables> permut){table[i++] = permut;});
	}
	uint64_t counts[VAR_FACTORIAL];
	for(uint64_t& v : counts) {v = 0;}

	std::default_random_engine generator;
	constexpr uint64_t SAMPLE_COUNT = 10000000;
	for(uint64_t sample_i = 0; sample_i < SAMPLE_COUNT; sample_i++) {
		BooleanFunction<Variables> sampleCopyCopy = sampleCopy;
		BooleanFunction<Variables> permut = permuter.permuteRandom(sampleCopyCopy, generator);
		for(size_t i = 0; i < VAR_FACTORIAL; i++) {
			if(table[i] == permut) {
				counts[i]++;
				goto next;
			}
		}
		std::cout << "Invalid permutation produced!" << std::endl;
		next:;
	}
	std::cout << "Done testing! Exact counts:";
	uint64_t min = SAMPLE_COUNT;
	uint64_t max = 0;
	for(size_t i = 0; i < VAR_FACTORIAL; i++) {
		uint64_t v = counts[i];
		if(v < min) min = v;
		if(v > max) max = v;
		if(i % 24 == 0) std::cout << "\n";
		std::cout << v << ", ";
	}
	std::cout << "\nmin: " << min << ", max: " << max << std::endl;
}

template<unsigned int Variables>
void benchmarkRandomMBFGeneration() {
	MBFSampler<Variables> sampler;
	FastRandomPermuter<Variables> permuter;

	testFastRandomPermuter(sampler.mbfsByClassSize[0].bf); // Will have 5040 variations

	std::cout << "Random Generation!" << std::endl;
	std::default_random_engine generator;

	uint64_t dont_optimize_summer = 0;
	constexpr uint64_t SAMPLE_SIZE = 1000000000;

	// Perform 3 benchmarks, one without permuteRandom, one with it once, and one with it twice.
	// This is to see what effect it has on runtime compared to the main memory read
	for(int numPermuteRandoms = 0; numPermuteRandoms < 3; numPermuteRandoms++) {
		auto start = std::chrono::high_resolution_clock::now();

		for(uint64_t i = 0; i < SAMPLE_SIZE; i++) {
			Monotonic<Variables> mbfFound = sampler.sample(generator);

			for(int i = 0; i < numPermuteRandoms; i++) {
				mbfFound.bf = permuter.permuteRandom(mbfFound.bf, generator);
			}

			if constexpr(Variables == 7) {
				dont_optimize_summer += _mm_extract_epi64(mbfFound.bf.bitset.data, 0) + _mm_extract_epi64(mbfFound.bf.bitset.data, 1);
			} else {
				dont_optimize_summer += mbfFound.bf.bitset.data;
			}
			//if(i % (SAMPLE_SIZE / 100) == 0) std::cout << i << std::endl;
		}
		auto time = std::chrono::high_resolution_clock::now() - start;
		uint64_t millis = std::chrono::duration_cast<std::chrono::milliseconds>(time).count();
		std::cout << "Took " << millis << "ms: " << (SAMPLE_SIZE / (millis / 1000.0)) << " MBF7 per second! with " << numPermuteRandoms << " permuteRandom() calls" << std::endl;
	}

	std::cout << "Don't optimize print: " << dont_optimize_summer << std::endl;
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

	{"countValidPermutationSetFraction7_0", []() {countValidPermutationSetFraction(std::vector<size_t>{10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000}, 0); }},
	{"countValidPermutationSetFraction7_1", []() {countValidPermutationSetFraction(std::vector<size_t>{10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000}, 1); }},
	{"countValidPermutationSetFraction7_2", []() {countValidPermutationSetFraction(std::vector<size_t>{10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000}, 2); }},
	{"countValidPermutationSetFraction7_3", []() {countValidPermutationSetFraction(std::vector<size_t>{10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000}, 3); }},
	{"countValidPermutationSetFraction7_4", []() {countValidPermutationSetFraction(std::vector<size_t>{10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000}, 4); }},
	{"countValidPermutationSetFraction7_5", []() {countValidPermutationSetFraction(std::vector<size_t>{10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000}, 5); }},
	{"countValidPermutationSetFraction7_6", []() {countValidPermutationSetFraction(std::vector<size_t>{10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000}, 6); }},
	{"countValidPermutationSetFraction7_7", []() {countValidPermutationSetFraction(std::vector<size_t>{10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000}, 7); }}
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

	{"estimateDedekind", [](const std::vector<std::string>& vars) {
		int n = std::stoi(vars[0]);

		for(int i = 0; i <= n; i++) {
			std::cout << "D(" << i << ") = " << estimateDedekindNumber(i) << std::endl;
		}
	}}
}};
