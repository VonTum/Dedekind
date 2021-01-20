
#include <iostream>
#include <fstream>
#include <random>
#include "dedekindDecomposition.h"
#include "valuedDecomposition.h"
#include "toString.h"

#include "timeTracker.h"
#include "codeGen.h"

#include "MBFDecomposition.h"

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
static FunctionInputBitSet<Variables> generateFibs() {
	return FunctionInputBitSet<Variables>(generateBitSet<(1 << Variables)>());
}

std::string getFileName(const char* title, unsigned int Variables, const char* extention) {
	std::string name(title);
	name.append(std::to_string(Variables));
	name.append(extention);
	return name;
}

template<unsigned int Variables>
void runGenAllMBFs() {
	std::pair<BufferedSet<FunctionInputBitSet<Variables>>, size_t> resultPair;
	{
		TimeTracker timer;
		resultPair = generateAllMBFsFast<Variables>();
	}
	BufferedSet<FunctionInputBitSet<Variables>>& result = resultPair.first;
	size_t numberOfLinks = resultPair.second;
	{
		std::cout << "Sorting\n";
		TimeTracker timer;
		std::sort(result.begin(), result.end(), [](FunctionInputBitSet<Variables>& a, FunctionInputBitSet<Variables>& b) -> bool {return a.size() < b.size(); });
	}


	uint64_t sizeCounts[(1 << Variables) + 1];
	for(size_t i = 0; i < (1 << Variables) + 1; i++) {
		sizeCounts[i] = 0;
	}
	for(const FunctionInputBitSet<Variables>& item : result) {
		sizeCounts[item.size()]++;
	}

	std::cout << "R(" << Variables << ") == " << result.size() << "\n";

	{
		std::ofstream file(getFileName("allUniqueMBF", Variables, ".mbf"), std::ios::binary);

		for(const FunctionInputBitSet<Variables>& item : result) {
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


void doRAMTest() {
	constexpr size_t numCurs = 32;
	constexpr uint32_t size = 1 << 25;

	std::default_random_engine generator;
	std::uniform_int_distribution<uint32_t> distribution(0, size - 1);

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
	
	uint32_t curs[numCurs];
	for(size_t i = 0; i < numCurs; ) {
		uint32_t selected = distribution(generator);
		if(jumpTable[selected] != 0) {
			curs[i] = selected;
			i++;
		}
	}
	constexpr size_t numHops = 1ULL << 26;
	std::cout << "Starting\n";
	std::chrono::nanoseconds finalTime;
	{
		TimeTracker timer;

		for(size_t i = 0; i < numHops; i++) {
			for(size_t c = 0; c < numCurs; c++) {
				_mm_prefetch(reinterpret_cast<const char*>(jumpTable + curs[c]), _MM_HINT_ENTA);
			}
			for(size_t c = 0; c < numCurs; c++) {
				curs[c] = jumpTable[curs[c]];
			}
		}

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
	std::ofstream intervalStatsFile(getFileName("intervalStats", Variables, ".txt"));

	intervalStatsFile << "layer, count, intervalSize, layerSize\n";
	for(size_t layer = 0; layer < (1 << Variables); layer++) {
		std::uniform_int_distribution<unsigned int> random(0, getLayerSize<Variables>(layer) - 1);
		std::default_random_engine generator;
		unsigned int selectedInLayer = random(generator);
		std::cout << "\n" << selectedInLayer << "\n";
		//readAllLinks<Variables>();
		std::pair<uint64_t, uint64_t> countAndIntervalSize = computeIntervalSize<Variables>(layer, selectedInLayer);
		uint64_t count = countAndIntervalSize.first;
		uint64_t iSize = countAndIntervalSize.second;

		intervalStatsFile << layer << ", " << count << ", " << iSize << ", " << getLayerSize<Variables>(layer) << "\n";
	}

	intervalStatsFile.close();
}


#ifndef RUN_TESTS
int main() {
	//doRAMTest();

	//doLinkCount<7>();

	//runGenAllMBFs<7>();
	
	//runSortAndComputeLinks<7>();
	
	//runGenLayerDecomposition();
	
	sampleIntervalSizes<7>();
}
#endif
