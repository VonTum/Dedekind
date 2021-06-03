
#include <thread>
#include "../dedelib/toString.h"
#include "../dedelib/sjomn.h"
#include "../dedelib/cmdParser.h"
#include "../dedelib/fileNames.h"
#include "../dedelib/allMBFsMap.h"
#include "../dedelib/pcoeff.h"


template<unsigned int Variables>
void printAllMBFs() {
	std::ifstream mbfFile(FileName::allMBFSSorted(Variables), std::ios::binary);
	if(!mbfFile.is_open()) throw "Error not found!";

	AllMBFMap<Variables, uint64_t> allMBFs = AllMBFMap<Variables, uint64_t>::readKeysFile(mbfFile);

	int layerI = 0;
	for(auto& layer : allMBFs) {
		std::cout << "Size " << layerI << std::endl;
		for(const KeyValue<Monotonic<Variables>, uint64_t>& element : layer) {
			std::cout << "    " << element.key << std::endl;
		}
		layerI++;
	}
}

size_t sizeTable[36];

template<unsigned int Variables>
void countAverageACSize() {
	std::ifstream benchmarkSetFile(FileName::benchmarkSet(Variables), std::ios::binary);
	if(!benchmarkSetFile.is_open()) throw "File not opened!";

	std::vector<std::pair<Monotonic<Variables>, IntervalSymmetry>> data;

	while(benchmarkSetFile.good()) {
		Monotonic<Variables> mbf = deserializeMBF<Variables>(benchmarkSetFile);
		IntervalSymmetry is = deserializeExtraData(benchmarkSetFile);
		data.push_back(std::make_pair(mbf, is));
		if(data.size() > 10000000) break;
	}

	for(const std::pair<Monotonic<Variables>, IntervalSymmetry>& item : data) {
		int count = item.first.asAntiChain().size();
		sizeTable[count]++;
	}

	for(int i = 0; i <= getMaxLayerWidth(Variables); i++) {
		std::cout << i << ": " << sizeTable[i] << "\n";
	}
}

template<unsigned int Variables>
void computeValidPCoeffFraction(int botFraction = 100) {
	std::vector<TopBots<Variables>> topBots = readTopBots<Variables>();
	std::cout << topBots.size() << " topBots found!" << std::endl;

	uint64_t validCount = 0;
	uint64_t invalidCount = 0;

	std::default_random_engine generator;
	std::uniform_int_distribution choiceChance(0, botFraction);
	int i = 0;
	for(const TopBots<Variables>& tb : topBots) {
		i++;
		if(i % 1000 == 0) {
			std::cout << i << "/" << topBots.size() << std::endl;
		}
		for(const Monotonic<Variables>& bot : tb.bots) {
			if(choiceChance(generator) != 0) continue;

			bot.forEachPermutation([&](const Monotonic<Variables>& permutedBot) {
				if(permutedBot <= tb.top) {
					validCount++;
				} else {
					invalidCount++;
				}
			});
		}
	}

	std::cout << "validCount: " << validCount << ", invalidCount: " << invalidCount << " fraction: " << (100.0 * validCount) / (validCount + invalidCount) << "%" << std::endl;
}
int main(int argc, const char** argv) {
	std::cout << "Detected " << std::thread::hardware_concurrency() << " threads" << std::endl;

	ParsedArgs parsed(argc, argv);

	std::string dataDir = parsed.getOptional("dataDir");
	if(!dataDir.empty()) {
		FileName::setDataPath(dataDir);
	}

	//sjomnumbertablesymmetric<2>([]() {});
	//printAllMBFs<4>();
	//countAverageACSize<7>();
	computeValidPCoeffFraction<7>(100);
}
