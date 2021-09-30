#pragma once

#include "flatMBFStructure.h"




template<unsigned int Variables>
void flatDPlus1() {
	FlatMBFStructure<Variables> s = readFlatMBFStructure<Variables>(false, true, false, false);

	u128 totalSum = 0;
	for(size_t i = 0; i < FlatMBFStructure<Variables>::MBF_COUNT; i++) {
		ClassInfo<Variables> ci = s.allClassInfos[i];
		totalSum += umul128(uint64_t(ci.intervalSizeDown), uint64_t(ci.classSize));
	}
	std::cout << "D(" << (Variables+1) << ") = " << totalSum << std::endl;
}



