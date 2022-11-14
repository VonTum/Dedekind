#include "flatPCoeff.h"

#include "flatBufferManagement.h"
#include "fileNames.h"


void flatDPlus1(unsigned int Variables) {
	const ClassInfo* allClassInfos = readFlatBuffer<ClassInfo>(FileName::flatClassInfo(Variables), mbfCounts[Variables]);

	u128 totalSum = 0;
	for(size_t i = 0; i < mbfCounts[Variables]; i++) {
		ClassInfo ci = allClassInfos[i];
		totalSum += umul128(uint64_t(ci.intervalSizeDown), uint64_t(ci.classSize));
	}
	std::cout << "D(" << (Variables+1) << ") = " << toString(totalSum) << std::endl;
}

void isEvenPlus2(unsigned int Variables) {
	const FlatNode* allNodes = readFlatBuffer<FlatNode>(FileName::flatNodes(Variables), mbfCounts[Variables]);
	const ClassInfo* allClassInfos = readFlatBuffer<ClassInfo>(FileName::flatClassInfo(Variables), mbfCounts[Variables]);

	bool isEven = true; // 0 is even
	for(NodeIndex i = 0; i < mbfCounts[Variables]; i++) {
		uint64_t classSize = allClassInfos[i].classSize;

		if(classSize % 2 == 0) continue;

		uint64_t intervalSizeDown = allClassInfos[i].intervalSizeDown;
		if(intervalSizeDown % 2 == 0) continue;

		NodeIndex dualI = allNodes[i].dual;
		uint64_t intervalSizeUp = allClassInfos[dualI].intervalSizeDown;
		if(intervalSizeUp % 2 == 0) continue;

		isEven = !isEven;
	}

	std::cout << "D(" << (Variables + 2) << ") is " << (isEven ? "even" : "odd") << std::endl;

	freeFlatBuffer<FlatNode>(allNodes, mbfCounts[Variables]);
	freeFlatBuffer<ClassInfo>(allClassInfos, mbfCounts[Variables]);
}

u192 computeDedekindNumberFromStandardBetaTopSums(unsigned int Variables, const u128* topSums) {
	const FlatNode* allNodes = readFlatBuffer<FlatNode>(FileName::flatNodes(Variables), mbfCounts[Variables]);
	const ClassInfo* allClassInfos = readFlatBuffer<ClassInfo>(FileName::flatClassInfo(Variables), mbfCounts[Variables]);

	u192 total = 0;

	for(size_t i = 0; i < mbfCounts[Variables]; i++) {
		ClassInfo topDualInfo = allClassInfos[allNodes[i].dual];
		uint64_t topIntervalSizeUp = topDualInfo.intervalSizeDown;
		uint64_t topFactor = topIntervalSizeUp * topDualInfo.classSize; // max log2(2414682040998*5040) = 53.4341783883
		total += umul192(topSums[i], topFactor);
	}

	freeFlatBuffer<FlatNode>(allNodes, mbfCounts[Variables]);
	freeFlatBuffer<ClassInfo>(allClassInfos, mbfCounts[Variables]);

	return total;
}

u192 computeDedekindNumberFromBetaSums(unsigned int Variables, const std::vector<BetaSumPair>& betaSums) {
	const FlatNode* allNodes = readFlatBuffer<FlatNode>(FileName::flatNodes(Variables), mbfCounts[Variables]);
	const ClassInfo* allClassInfos = readFlatBuffer<ClassInfo>(FileName::flatClassInfo(Variables), mbfCounts[Variables]);

	if(betaSums.size() != mbfCounts[Variables]) {
		std::cerr << "Incorrect number of beta sums! Aborting!" << std::endl;
		std::abort();
	}
	u192 total = 0;
	for(size_t i = 0; i < betaSums.size(); i++) {
		BetaSumPair bPair = betaSums[i];
		BetaSum betaSum = bPair.getBetaSum(Variables);
		ClassInfo topInfo = allClassInfos[i];
		
		// invalid for DEDUPLICATION
#ifndef PCOEFF_DEDUPLICATE
		if(betaSum.countedIntervalSizeDown != topInfo.intervalSizeDown) throw "INVALID!";
#endif
		uint64_t topIntervalSizeUp = allClassInfos[allNodes[i].dual].intervalSizeDown;
		uint64_t topFactor = topIntervalSizeUp * topInfo.classSize; // max log2(2414682040998*5040) = 53.4341783883
		total += umul192(betaSum.betaSum, topFactor);
	}


	freeFlatBuffer<FlatNode>(allNodes, mbfCounts[Variables]);
	freeFlatBuffer<ClassInfo>(allClassInfos, mbfCounts[Variables]);
	
	return total;
}
