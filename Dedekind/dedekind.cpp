
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
	FunctionInputBitSet<Variables>* allFibs;
	FunctionInputBitSet<Variables>* allFibsEnd;
	{
		TimeTracker timer;
		auto pair = generateAllMBFs<Variables>();
		allFibs = pair.first;
		allFibsEnd = pair.second;
	}

	std::cout << "R(" << Variables << ") == " << (allFibsEnd - allFibs) << "\n";

	std::string name = "allUniqueMBF";
	name.append(std::to_string(Variables));
	name.append(".mbf");
	std::ofstream file(name, std::ios::binary);

	uint8_t buf[(1 << Variables) / 8];
	for(FunctionInputBitSet<Variables>* item = allFibs; item != allFibsEnd; item++) {
		serialize(*item, buf);
		file.write(reinterpret_cast<char*>(buf), (1 << Variables) / 8);
	}
	file.close();
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
