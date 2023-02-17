#pragma once

#include "pcoeffClasses.h"
#include "flatPCoeff.h"
#include "u192.h"

#include <atomic>
#include <mutex>
#include <iostream>


struct SingleTopResult {
	BetaSum resultSum;
	BetaSum dualSum;
	BetaSum validationSum;
};

struct SingleTopPThreadData {
	alignas(64) std::atomic<NodeIndex> curIndex;
	NodeIndex topIdx;
	NodeIndex topDual;
	unsigned int Variables;
	const void* mbfLUT;
	const ClassInfo* classInfos;
	
	alignas(64) std::mutex resultMutex;
	SingleTopResult result;
	
	SingleTopPThreadData(unsigned int Variables);
	void run(NodeIndex topIdx, void*(*func)(void*));
	~SingleTopPThreadData();
};

template<unsigned int Variables, bool SkipValidationSum = true>
void* computeSingleTopWithAllCoresPThread(void* voidData) {
	constexpr NodeIndex BLOCK_SIZE = (Variables <= 6) ? 16 : 512;
	constexpr NodeIndex REPORT_EVERY = (Variables <= 6) ? 1024 : 1024*1024;

	SingleTopPThreadData* data = (SingleTopPThreadData*) voidData;

	const Monotonic<Variables>* mbfLUT = (const Monotonic<Variables>*) data->mbfLUT;
	const ClassInfo* classInfos = data->classInfos;
	NodeIndex topIndex = data->topIdx;
	NodeIndex topDual = data->topDual;
	NodeIndex endAt = topIndex;
	if(SkipValidationSum && endAt > topDual) endAt = topDual - 1;

	std::atomic<NodeIndex>& curIndex = data->curIndex;
	
	Monotonic<Variables> top = mbfLUT[topIndex];

	BetaSum resultSum{0, 0};
	BetaSum validationSum{0, 0};
	BooleanFunction<Variables> graphBuf[factorial(Variables)];
	while(true) {
		NodeIndex claimedSet = curIndex.fetch_add(BLOCK_SIZE);
		if(claimedSet % REPORT_EVERY == 0) {
			std::cout << std::to_string(claimedSet) + "\n" << std::flush;
		}
		if(claimedSet > endAt) break;
		NodeIndex claimedSetEnd = claimedSet + BLOCK_SIZE;
		if(claimedSetEnd > endAt+1) {
			claimedSetEnd = endAt+1;
		}
		for(NodeIndex botIdx = claimedSet; botIdx < claimedSetEnd; botIdx++) {
			if(botIdx == topDual) continue;

			bool isValidation = botIdx > topDual;
			// if(SkipValidationSum && isValidation) continue; // Not needed, should be covered more efficiently by endAt, but this is the correct logic
			Monotonic<Variables> bot = mbfLUT[botIdx];
			ProcessedPCoeffSum result = processPCoeffSum(top, bot, graphBuf);
			ClassInfo botInfo = classInfos[botIdx];
			BetaSum subSum = produceBetaTerm(botInfo, result);

			if(isValidation) {
				validationSum += subSum;
			} else {
				resultSum += subSum;
			}
		}
	}

	data->resultMutex.lock();
	data->result.resultSum += resultSum;
	data->result.validationSum += validationSum;
	data->resultMutex.unlock();

	pthread_exit(nullptr);
	return nullptr;
}

template<unsigned int Variables, bool SkipValidationSum = true>
SingleTopResult computeSingleTopWithAllCores(NodeIndex topIdx) {
	std::cout << "Computing correct results for top " + std::to_string(topIdx) << std::endl;

	SingleTopPThreadData data(Variables);
	if(topIdx != mbfCounts[Variables]-1) {
		data.run(topIdx, computeSingleTopWithAllCoresPThread<Variables, SkipValidationSum>);
		ClassInfo dualInfo = data.classInfos[data.topDual];
		const Monotonic<Variables>* mbfLUT = (const Monotonic<Variables>*) data.mbfLUT;
		ProcessedPCoeffSum result = processPCoeffSum(mbfLUT[data.topIdx], mbfLUT[data.topDual]);
		data.result.dualSum = produceBetaTerm(dualInfo, result);
		//data.result.resultSum -= data.result.dualSum;
	} else {
		data.result.resultSum.betaSum = 0;
		data.result.resultSum.countedIntervalSizeDown = 0;
		data.result.dualSum.betaSum = 2*factorial(Variables);
		data.result.dualSum.countedIntervalSizeDown = factorial(Variables);
	}
	return data.result;
}
