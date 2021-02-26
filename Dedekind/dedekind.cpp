
#include <iostream>
#include <fstream>
#include <random>
#include "dedekindDecomposition.h"
#include "valuedDecomposition.h"
#include "toString.h"
#include "serialization.h"

#include "timeTracker.h"
#include "codeGen.h"

#include "MBFDecomposition.h"
#include "r8Computation.h"
#include "tjomn.h"

/*
Correct numbers
	0: 2
	1: 3
	2: 6
	3: 20
	4: 168
	5: 7581
	6: 7828354
	7: 2414682040998
	8: 56130437228687557907788
	9: ??????????????????????????????????????????
*/







static bool genBool() {
	return rand() % 2 == 1;
}

template<size_t Size>
static BitSet<Size> generateBitSet() {
	BitSet<Size> bs;

	for(size_t i = 0; i < Size; i++) {
		if(genBool()) {
			bs.set(i);
		}
	}

	return bs;
}

template<unsigned int Variables>
static BooleanFunction<Variables> generateFibs() {
	return BooleanFunction<Variables>(generateBitSet<(1 << Variables)>());
}

std::string getFileName(const char* title, unsigned int Variables, const char* extention) {
	std::string name(title);
	name.append(std::to_string(Variables));
	name.append(extention);
	return name;
}

std::string getFileName(std::string title, unsigned int Variables, const char* extention) {
	std::string name(title);
	name.append(std::to_string(Variables));
	name.append(extention);
	return name;
}

template<unsigned int Variables>
void runGenAllMBFs() {
	std::pair<BufferedSet<Monotonic<Variables>>, size_t> resultPair;
	{
		TimeTracker timer;
		resultPair = generateAllMBFsFast<Variables>();
	}
	BufferedSet<Monotonic<Variables>>& result = resultPair.first;
	size_t numberOfLinks = resultPair.second;
	{
		std::cout << "Sorting\n";
		TimeTracker timer;
		std::sort(result.begin(), result.end(), [](Monotonic<Variables>& a, Monotonic<Variables>& b) -> bool {return a.size() < b.size(); });
	}


	uint64_t sizeCounts[(1 << Variables) + 1];
	for(size_t i = 0; i < (1 << Variables) + 1; i++) {
		sizeCounts[i] = 0;
	}
	for(const Monotonic<Variables>& item : result) {
		sizeCounts[item.size()]++;
	}

	std::cout << "R(" << Variables << ") == " << result.size() << "\n";

	{
		std::ofstream file(getFileName("allUniqueMBF", Variables, ".mbf"), std::ios::binary);

		for(const Monotonic<Variables>& item : result) {
			serializeMBF(item, file);
		}
		file.close();
	}

	{
		std::ofstream file(getFileName("allUniqueMBF", Variables, "info.txt"));


		file << "R(" << Variables << ") == " << result.size() << "\n";

		std::cout << "numberOfLinks: " << numberOfLinks << "\n";
		file << "numberOfLinks: " << numberOfLinks << "\n";

		//std::cout << "classesPerSize: " << sizeCounts[0];
		file << "classesPerSize: " << sizeCounts[0];
		for(size_t i = 1; i < (1 << Variables) + 1; i++) {
			//std::cout << ", " << sizeCounts[i] << "\n";
			file << ", " << sizeCounts[i];
		}
		file << "\n";
		file.close();
	}
}

void runGenLayerDecomposition() {
	std::cout << "Detected " << std::thread::hardware_concurrency() << " available threads!\n";
	TimeTracker timer;
	int dedekindOrder = 7;
	DedekindDecomposition<ValueCounted> fullDecomposition(dedekindOrder);

	//std::cout << "Decomposition:\n" << fullDecomposition << "\n";

	std::cout << "Dedekind " << dedekindOrder << " = " << fullDecomposition.getDedekind() << std::endl;
}

template<unsigned int Variables>
void runSortAndComputeLinks() {
	TimeTracker timer;

	std::ifstream inputFile(getFileName("allUniqueMBF", Variables, ".mbf"), std::ios::binary);
	std::ofstream sortedFile(getFileName("allUniqueMBFSorted", Variables, ".mbf"), std::ios::binary);
	std::ofstream linkFile(getFileName("mbfLinks", Variables, ".mbfLinks"), std::ios::binary);

	sortAndComputeLinks<Variables>(inputFile, sortedFile, linkFile);

	inputFile.close();
	sortedFile.close();
	linkFile.close();
}

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
	constexpr std::size_t size = std::size_t(1) << 30;

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

	std::ifstream linkFile(getFileName("mbfLinks", Variables, ".mbfLinks"), std::ios::binary);
	std::ofstream linkStatsFile(getFileName("linkStats", Variables, ".txt"));

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
	/*std::ofstream intervalStatsFile(getFileName("intervalStats", Variables, ".txt"));

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

		intervalStatsFile.open(getFileName("intervalStats", Variables, ".txt"), std::ios::app);
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
	swapper.add(nodeIndex, 1);
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

				swapper.add(ln.index, ln.count * count * multiplier);
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

			int numSubSets = countSuperSetPermutations(cur.func, start.func);
			if(numSubSets < 1) {
				__debugbreak();
				int numSubSets = countSuperSetPermutations(cur.func, start.func);
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
void saveIntervalSizes() {
	MBFDecomposition<Variables> dec = readFullMBFDecomposition<Variables>();

	std::ofstream intervalSizeFile(getFileName("intervalSizesToTop", Variables, ".mbfU64"), std::ios::binary);
	std::ofstream classCountFile(getFileName("uniqueClassCountsToTop", Variables, ".mbfU64"), std::ios::binary);

	for(int layer = 0; layer < (1 << Variables) + 1; layer++) {
		for(int curItemInLayer = 0; curItemInLayer < getLayerSize<Variables>(layer); curItemInLayer++) {
			std::pair<uint64_t, uint64_t> intervalSize = getIntervalSizeFor(dec, layer, curItemInLayer);
			std::cout << "for " << layer << ", " << curItemInLayer << ": intervalSize: " << intervalSize.first << " uniqueClassesSize: " << intervalSize.second << "\n";

			uint8_t buf[8];
			serializeU64(intervalSize.first, buf);
			intervalSizeFile.write(reinterpret_cast<const char*>(buf), 8);
			serializeU64(intervalSize.second, buf);
			classCountFile.write(reinterpret_cast<const char*>(buf), 8);
		}
	}

	intervalSizeFile.close();
	classCountFile.close();
}
/*
template<unsigned int Variables>
void computeDualForFile(size_t elementSize, std::ifstream& originalFile, std::ofstream& dualFile) {
	std::ifstream mbfFile(getFileName("allUniqueMBFSorted", Variables, ".mbf"), std::ios::binary);
	MBFHashSet<Variables> fullMBFSet(mbfFile);
	mbfFile.close();

	constexpr size_t size = mbfCounts[Variables];

	char* originalBuf = new char[size * elementSize];
	originalFile.read(originalBuf, size * elementSize);

	for(size_t layer = 0; layer < (1 << Variables) + 1; layer++) {
		size_t dualLayer = (1 << Variables) - layer;
		for(const BooleanFunction<Variables>& destFibs : fullMBFSet.bufSets[layer]) {
			BooleanFunction<Variables>* dualIndex = fullMBFSet.bufSets[dualLayer].find(destFibs.dual());

			size_t index = dualIndex - fullMBFSet.mbfs;

			dualFile.write(originalBuf + index * elementSize, elementSize);
		}
	}

	delete[] originalBuf;
}

template<unsigned int Variables>
void produceDualU64File(const char* inputName, const char* dualName) {
	std::ifstream inputFile(getFileName(inputName, Variables, ".mbfU64"), std::ios::binary);
	std::ofstream outputFile(getFileName(dualName, Variables, ".mbfU64"), std::ios::binary);

	computeDualForFile<Variables>(8, inputFile, outputFile);
}
*/

template<unsigned int DedekindOrder>
void doRevolution() {
	std::cout << "Computing D(" << DedekindOrder << ")...\n";

	TimeTracker timer;

	revolutionParallel<DedekindOrder - 3>();
}

#include "bigint/uint256_t.h"

#ifndef RUN_TESTS
int main() {
	//doRAMTest();  // works

	//MBFDecompositionWithHash<6> thing; // doesn't work

	//runGenAllMBFs<5>(); // works
	
	//runSortAndComputeLinks<5>(); // works

	//saveIntervalSizes<5>(); // works

	//doLinkCount<5>(); // works

	//runGenLayerDecomposition(); // works
	
	//sampleIntervalSizes<5>(); // works

	//std::cout << "D=" << getD<6>() << "\n"; // works
	//std::cout << "R=" << getR<6>(); // doesn't work

	/*IntervalSizeCache<1>::generate(generateAllMBFsFast<1>().first);
	IntervalSizeCache<2>::generate(generateAllMBFsFast<2>().first);
	IntervalSizeCache<3>::generate(generateAllMBFsFast<3>().first);
	IntervalSizeCache<4>::generate(generateAllMBFsFast<4>().first);
	IntervalSizeCache<5>::generate(generateAllMBFsFast<5>().first);
	IntervalSizeCache<6>::generate(generateAllMBFsFast<6>().first);*/

	doRevolution<4>();
	doRevolution<5>();
	doRevolution<6>();
	doRevolution<7>();
	doRevolution<8>();
	//doRevolution<9>();
}
#endif
