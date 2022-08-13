#include "resultCollection.h"

#include "knownData.h"

#include "flatBufferManagement.h"
#include "fileNames.h"

#include <iostream>
#include <string>

// does the necessary math with annotated number of bits, no overflows possible for D(9). 
BetaSum produceBetaTerm(ClassInfo info, uint64_t pcoeffSum, uint64_t pcoeffCount) {
	// the multiply is max log2(2^35 * 5040 * 5040) = 59.5984160368 bits long, fits in 64 bits
	// Compiler optimizes the divide by compile-time constant VAR_FACTORIAL to an imul, much faster!
	uint64_t deduplicatedTotalPCoeffSum = pcoeffSum * info.classSize; 
	u128 betaTerm = umul128(deduplicatedTotalPCoeffSum, info.intervalSizeDown); // no overflow data loss 64x64->128 bits

	// validation data
	uint64_t deduplicatedCountedPermutes = pcoeffCount * info.classSize;

	return BetaSum{betaTerm, deduplicatedCountedPermutes};
}

BetaSum sumOverBetas(const ClassInfo* mbfClassInfos, const NodeIndex* idxBuf, const NodeIndex* bufEnd, const ProcessedPCoeffSum* countConnectedSumBuf) {
	BetaSum total = BetaSum{0,0};

	for(const NodeIndex* cur = idxBuf; cur != bufEnd; cur++) {
		ClassInfo info = mbfClassInfos[*cur];

		ProcessedPCoeffSum processedPCoeff = *countConnectedSumBuf++;

		if((processedPCoeff & 0x8000000000000000) != uint64_t(0)) {
			std::cerr << "ECC ERROR DETECTED! At bot Index " << (cur - idxBuf) << ", value was: " << processedPCoeff << std::endl;
			throw "ECC ERROR DETECTED!";
		}
		uint64_t pcoeffSum = getPCoeffSum(processedPCoeff);
		uint64_t pcoeffCount = getPCoeffCount(processedPCoeff);

		total += produceBetaTerm(info, pcoeffSum, pcoeffCount);
	}
	return total;
}


BetaSum produceBetaResult(unsigned int Variables, const ClassInfo* mbfClassInfos, const JobInfo& curJob, const ProcessedPCoeffSum* pcoeffSumBuf) {
	// Skip the first elements, as it is the top
	BetaSum jobSum = sumOverBetas(mbfClassInfos, curJob.bufStart + BUF_BOTTOM_OFFSET, curJob.end(), pcoeffSumBuf + BUF_BOTTOM_OFFSET);

#ifdef PCOEFF_DEDUPLICATE
	ProcessedPCoeffSum nonDuplicateTopDual = pcoeffSumBuf[TOP_DUAL_INDEX]; // Index of dual

	ClassInfo info = mbfClassInfos[curJob.bufStart[TOP_DUAL_INDEX]];

	BetaSum nonDuplicateTopDualResult = produceBetaTerm(info, getPCoeffSum(nonDuplicateTopDual), getPCoeffCount(nonDuplicateTopDual));

	jobSum = jobSum + jobSum + nonDuplicateTopDualResult;
#endif
	return jobSum / factorial(Variables);
}

const ClassInfo* loadClassInfos(unsigned int Variables) {
	return readFlatBuffer<ClassInfo>(FileName::flatClassInfo(Variables), mbfCounts[Variables]);
}

void resultProcessor(
	unsigned int Variables,
	SynchronizedQueue<OutputBuffer>& outputQueue,
	SynchronizedMultiNUMAAlloc<NodeIndex>& inputBufferReturnQueue,
	SynchronizedStack<ProcessedPCoeffSum*>& outputBufferReturnQueue,
	std::vector<BetaResult>& finalResults
) {
	std::cout << "\033[32m[Result Processor] Started loading ClassInfos...\033[39m\n" << std::flush;
	const ClassInfo* mbfClassInfos = loadClassInfos(Variables);
	std::cout << "\033[32m[Result Processor] Finished Loading ClassInfos. Result processor started.\033[39m\n" << std::flush;
	for(std::optional<OutputBuffer> outputBuffer; (outputBuffer = outputQueue.pop_wait()).has_value(); ) {
		OutputBuffer outBuf = outputBuffer.value();

		BetaResult curBetaResult;
		//if constexpr(Variables == 7) std::cout << "Results for job " << outBuf.originalInputData.getTop() << std::endl;
		curBetaResult.topIndex = outBuf.originalInputData.getTop();
		curBetaResult.betaSum = produceBetaResult(Variables, mbfClassInfos, outBuf.originalInputData, outBuf.outputBuf);

		inputBufferReturnQueue.free(std::move(outBuf.originalInputData.bufStart));
		outputBufferReturnQueue.push(outBuf.outputBuf);

		finalResults.push_back(curBetaResult);
		if(finalResults.size() % 1000 == 0) std::cout << "\033[32m[Result Processor] " + std::to_string(finalResults.size()) + "\033[39m\n" << std::flush;
	}
	std::cout << "\033[32m[Result Processor] Result processor finished.\033[39m\n" << std::flush;
}

