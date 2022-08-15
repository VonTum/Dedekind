#include "resultCollection.h"

#include "knownData.h"

#include "flatBufferManagement.h"
#include "fileNames.h"
#include "processingContext.h"
#include "threadUtils.h"

#include <iostream>
#include <string>
#include <atomic>

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

void resultprocessingThread(
	unsigned int Variables,
	const ClassInfo* mbfClassInfos,
	PCoeffProcessingContext& context,
	std::atomic<BetaResult*>& resultPtr,
	void(*validator)(const OutputBuffer&, const void*),
	const void* validatorData
) {
	std::cout << "\033[32m[Result Processor] Result processor Thread started.\033[39m\n" << std::flush;
	for(std::optional<OutputBuffer> outputBuffer; (outputBuffer = context.outputQueue.pop_wait()).has_value(); ) {
		OutputBuffer outBuf = outputBuffer.value();

		validator(outBuf, validatorData);

		BetaResult curBetaResult;
		//if constexpr(Variables == 7) std::cout << "Results for job " << outBuf.originalInputData.getTop() << std::endl;
		curBetaResult.topIndex = outBuf.originalInputData.getTop();
		curBetaResult.betaSum = produceBetaResult(Variables, mbfClassInfos, outBuf.originalInputData, outBuf.outputBuf);

		context.inputBufferAllocator.free(std::move(outBuf.originalInputData.bufStart));
		context.outputBufferReturnQueue.push(outBuf.outputBuf);

		BetaResult* allocatedSlot = resultPtr.fetch_add(1, std::memory_order_relaxed);
		*allocatedSlot = std::move(curBetaResult);
	}
	std::cout << "\033[32m[Result Processor] Result processor Thread finished.\033[39m\n" << std::flush;
}

std::vector<BetaResult> resultProcessor(
	unsigned int Variables,
	PCoeffProcessingContext& context,
	size_t numResults
) {
	std::cout << "\033[32m[Result Processor] Started loading ClassInfos...\033[39m\n" << std::flush;
	const ClassInfo* mbfClassInfos = loadClassInfos(Variables);
	std::cout << "\033[32m[Result Processor] Finished Loading ClassInfos. Result processor started.\033[39m\n" << std::flush;

	std::vector<BetaResult> finalResults(numResults);
	std::atomic<BetaResult*> finalResultPtr;
	finalResultPtr.store(&finalResults[0]);
	resultprocessingThread(Variables, mbfClassInfos, context, finalResultPtr, [](const OutputBuffer&, const void*){}, nullptr);

	std::cout << "\033[32m[Result Processor] Result processor finished.\033[39m\n" << std::flush;
	return finalResults;
}

std::vector<BetaResult> NUMAResultProcessorWithValidator(
	unsigned int Variables,
	PCoeffProcessingContext& context,
	size_t numResults,
	size_t numValidators,
	void(*validator)(const OutputBuffer&, const void*),
	const void* mbfs[2]
) {
	std::cout << "\033[32m[Result Processor] Started loading ClassInfos...\033[39m\n" << std::flush;
	const ClassInfo* mbfClassInfos = loadClassInfos(Variables);
	std::cout << "\033[32m[Result Processor] Finished Loading ClassInfos. Result processor started.\033[39m\n" << std::flush;

	std::cout << "\033[32m[Result Processor] Started loading ClassInfos...\033[39m\n" << std::flush;

	std::vector<BetaResult> finalResults(numResults);
	std::atomic<BetaResult*> finalResultPtr;
	finalResultPtr.store(&finalResults[0]);

	constexpr int MAX_VALIDATOR_COUNT = 16;

	struct ThreadData {
		unsigned int Variables;
		PCoeffProcessingContext* context;
		const ClassInfo* mbfClassInfos;
		std::atomic<BetaResult*>* finalResultPtr;
		void(*validator)(const OutputBuffer&, const void*);
		const void* mbfs;
	};
	ThreadData datas[2];
	for(int i = 0; i < 2; i++) {
		datas[i].Variables = Variables;
		datas[i].context = &context;
		datas[i].mbfClassInfos = mbfClassInfos;
		datas[i].finalResultPtr = &finalResultPtr;
		datas[i].validator = validator;
		datas[i].mbfs = mbfs[i];
	}

	auto threadFunc = [](void* voidData) -> void* {
		ThreadData* tData = (ThreadData*) voidData;
		resultprocessingThread(tData->Variables, tData->mbfClassInfos, *tData->context, *tData->finalResultPtr, tData->validator, tData->mbfs);
		pthread_exit(nullptr);
		return nullptr;
	};

	pthread_t validatorThreads[MAX_VALIDATOR_COUNT];
	for(int i = 0; i < numValidators; i++) {
		int socketIdx = i / 8;
		validatorThreads[i] = createCoreComplexPThread(i, threadFunc, (void*) &datas[socketIdx]);
	}

	for(int i = 0; i < numValidators; i++) {
		pthread_join(validatorThreads[i], nullptr);
	}

	std::cout << "\033[32m[Result Processor] Result processor finished.\033[39m\n" << std::flush;
	return finalResults;
}
