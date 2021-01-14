
#include <iostream>
#include <fstream>
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
		std::string name = "allUniqueMBF";
		name.append(std::to_string(Variables));
		name.append(".mbf");
		std::ofstream file(name, std::ios::binary);

		uint8_t buf[(1 << Variables) / 8];
		for(const FunctionInputBitSet<Variables>& item : result) {
			serialize(item, buf);
			file.write(reinterpret_cast<char*>(buf), (1 << Variables) / 8);
		}
		file.close();
	}

	{
		std::string name = "allUniqueMBF";
		name.append(std::to_string(Variables));
		name.append("info.txt");
		std::ofstream file(name);


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

#ifndef RUN_TESTS
int main() {
	runGenAllMBFs<7>();
	return 0;

	//runGenLayerDecomposition();
	//return 0;
}
#endif
