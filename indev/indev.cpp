
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

int main(int argc, const char** argv) {
	std::cout << "Detected " << std::thread::hardware_concurrency() << " threads" << std::endl;

	ParsedArgs parsed(argc, argv);

	std::string dataDir = parsed.getOptional("dataDir");
	if(!dataDir.empty()) {
		FileName::setDataPath(dataDir);
	}

	//sjomnumbertablesymmetric<2>([]() {});
	//printAllMBFs<4>();
	countAverageACSize<7>();
}
