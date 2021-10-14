
#include <thread>
#include "../dedelib/toString.h"
#include "../dedelib/sjomn.h"
#include "../dedelib/cmdParser.h"
#include "../dedelib/fileNames.h"
#include "../dedelib/allMBFsMap.h"
#include "../dedelib/pcoeff.h"

#include "../dedelib/flatMBFStructure.h"
#include "../dedelib/flatPCoeff.h"


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
	{
		ParsedArgs parsed(argc, argv);

		std::string dataDir = parsed.getOptional("dataDir");
		if(!dataDir.empty()) {
			FileName::setDataPath(dataDir);
		}
	}
	//sjomnumbertablesymmetric<2>([]() {});
	//printAllMBFs<4>();
	//countAverageACSize<7>();
	//computeValidPCoeffFraction<7>(100);
	//std::cout << getTotalLinkCount<7>();
	
	/*constexpr int Variables = 6;

	FlatMBFStructure<Variables> s = readFlatMBFStructure<Variables>();
	SwapperLayers<Variables, BitSet<32>> swapper;
	JobBatch<Variables, 32> jobBatch;

	NodeIndex tops[32]{5,9,7,351,6874,1684,3351,1354,3584,3524,3515,2235,8964,5486,2154,2121,2123,8476,4896,1768, 11000, 11500, 10869, 1472, 351, 2252, 5525, 4414, 6636, 6696, 5585, 4474};

	computeBuffers(s, jobBatch, swapper, tops, 32);*/

	Monotonic<7> bot;

	uint64_t resultingBotCount = 0;

	bot.forEachPermutation(3,7, [&](const Monotonic<7>& permBot) {
		resultingBotCount++;
	});
	std::cout << "Permuts: " << resultingBotCount << std::endl;

	//computeFlatDPlus2<6, 32>();
}
