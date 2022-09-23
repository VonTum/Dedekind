#include "resultCollection.h"

#include "knownData.h"

#include "flatBufferManagement.h"
#include "fileNames.h"
#include "processingContext.h"
#include "threadUtils.h"
#include "threadPool.h"
#include "numaMem.h"


#include <iostream>
#include <string>
#include <atomic>

#include <string.h>

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

BetaSum produceBetaTerm(ClassInfo info, ProcessedPCoeffSum processedPCoeff) {
	uint64_t pcoeffSum = getPCoeffSum(processedPCoeff);
	uint64_t pcoeffCount = getPCoeffCount(processedPCoeff);
	return produceBetaTerm(info, pcoeffSum, pcoeffCount);
}

// Does not take validation buffer
static BetaSum sumOverBetas(const ClassInfo* mbfClassInfos, const NodeIndex* idxBuf, const NodeIndex* bufEnd, const ProcessedPCoeffSum* countConnectedSumBuf) {
	BetaSum total = BetaSum{0,0};

	for(const NodeIndex* cur = idxBuf; cur != bufEnd; cur++) {
		ClassInfo info = mbfClassInfos[*cur];

		ProcessedPCoeffSum processedPCoeff = *countConnectedSumBuf++;

		if((processedPCoeff & 0x8000000000000000) != uint64_t(0)) {
			std::cerr << "ECC ERROR DETECTED! At bot Index " << (cur - idxBuf) << ", value was: " << processedPCoeff << std::endl;
			std::abort();
		}

		total += produceBetaTerm(info, processedPCoeff);
	}
	return total;
}

// Takes validation buffer
static BetaSum sumOverBetas(const ClassInfo* mbfClassInfos, const NodeIndex* idxBuf, const NodeIndex* bufEnd, const ProcessedPCoeffSum* countConnectedSumBuf, ClassInfo topDualClassInfo, ValidationData* validationBuf) {
	BetaSum total = BetaSum{0,0};

	for(const NodeIndex* cur = idxBuf; cur != bufEnd; cur++) {
		ClassInfo info = mbfClassInfos[*cur];

		ProcessedPCoeffSum processedPCoeff = *countConnectedSumBuf++;

		if((processedPCoeff & 0x8000000000000000) != uint64_t(0)) {
			std::cerr << "ECC ERROR DETECTED! At bot Index " << (cur - idxBuf) << ", value was: " << processedPCoeff << std::endl;
			std::abort();
		}

		total += produceBetaTerm(info, processedPCoeff);

		// This is the sum to be added to the top "inv(bot)", because we deduplicated it off from that top
		validationBuf[*cur].dualBetaSum += produceBetaTerm(topDualClassInfo, processedPCoeff);
	}
	return total;
}

// Optionally takes a buffer for validation data
static BetaSumPair produceBetaResult(const ClassInfo* mbfClassInfos, const JobInfo& curJob, const ProcessedPCoeffSum* pcoeffSumBuf, ValidationData* validationBuf = nullptr) {
	// Skip the first elements, as it is the top
	BetaSumPair result;
	if(validationBuf != nullptr) {
		NodeIndex topDualIdx = curJob.bufStart[TOP_DUAL_INDEX];
		ClassInfo topDualClassInfo = mbfClassInfos[topDualIdx];	

		result.betaSum = sumOverBetas(mbfClassInfos, curJob.bufStart + BUF_BOTTOM_OFFSET, curJob.end(), pcoeffSumBuf + BUF_BOTTOM_OFFSET, topDualClassInfo, validationBuf);
	} else {
		result.betaSum = sumOverBetas(mbfClassInfos, curJob.bufStart + BUF_BOTTOM_OFFSET, curJob.end(), pcoeffSumBuf + BUF_BOTTOM_OFFSET);
	}

#ifdef PCOEFF_DEDUPLICATE
	ProcessedPCoeffSum nonDuplicateTopDual = pcoeffSumBuf[TOP_DUAL_INDEX]; // Index of dual

	ClassInfo info = mbfClassInfos[curJob.bufStart[TOP_DUAL_INDEX]];

	result.betaSumDualDedup = produceBetaTerm(info, getPCoeffSum(nonDuplicateTopDual), getPCoeffCount(nonDuplicateTopDual));
#endif
	return result;
}

static const ClassInfo* loadClassInfos(unsigned int Variables) {
	return readFlatBuffer<ClassInfo>(FileName::flatClassInfo(Variables), mbfCounts[Variables]);
}

static void resultprocessingThread(
	const ClassInfo* mbfClassInfos,
	PCoeffProcessingContext& context,
	std::atomic<BetaResult*>& resultPtr,
	ValidationData* validationData
) {
	std::cout << "\033[32m[Result Processor] Result processor Thread started.\033[39m\n" << std::flush;
	for(std::optional<OutputBuffer> outputBuffer; (outputBuffer = context.outputQueue.pop_wait()).has_value(); ) {
		OutputBuffer outBuf = outputBuffer.value();

		BetaResult curBetaResult;
		//if constexpr(Variables == 7) std::cout << "Results for job " << outBuf.originalInputData.getTop() << std::endl;
		curBetaResult.topIndex = outBuf.originalInputData.getTop();

		curBetaResult.dataForThisTop = produceBetaResult(mbfClassInfos, outBuf.originalInputData, outBuf.outputBuf, validationData);

		context.validationQueue.push(outBuf);

		BetaResult* allocatedSlot = resultPtr.fetch_add(1);
		*allocatedSlot = std::move(curBetaResult);
	}
	context.validationQueue.close();
	std::cout << "\033[32m[Result Processor] Result processor Thread finished.\033[39m\n" << std::flush;
}

ResultProcessorOutput NUMAResultProcessor(
	unsigned int Variables,
	PCoeffProcessingContext& context,
	size_t numResults
) {
	std::cout << "\033[32m[Result Processor] Started loading ClassInfos...\033[39m\n" << std::flush;
	const ClassInfo* mbfClassInfos = loadClassInfos(Variables);
	std::cout << "\033[32m[Result Processor] Finished Loading ClassInfos. Result processor started.\033[39m\n" << std::flush;

	ResultProcessorOutput result;
	result.results.resize(numResults);
	std::atomic<BetaResult*> finalResultPtr;
	finalResultPtr.store(&result.results[0]);

	void* validationBuffers[2];
	size_t validationBufferSize = sizeof(ValidationData) * VALIDATION_BUFFER_SIZE(Variables);
	allocSocketBuffers(validationBufferSize, validationBuffers);
	memset(validationBuffers[0], 0, validationBufferSize);
	memset(validationBuffers[1], 0, validationBufferSize);

	struct ThreadData {
		PCoeffProcessingContext* context;
		const ClassInfo* mbfClassInfos;
		std::atomic<BetaResult*>* finalResultPtr;
		ValidationData* validationBuffer;
	};
	ThreadData datas[2];
	for(int i = 0; i < 2; i++) {
		datas[i].context = &context;
		datas[i].mbfClassInfos = mbfClassInfos;
		datas[i].finalResultPtr = &finalResultPtr;
		datas[i].validationBuffer = static_cast<ValidationData*>(validationBuffers[i]);
	}

	{// Scope so that the result processing threads exit before continuing
		PThreadsSpread threads(2, CPUAffinityType::SOCKET, datas, [](void* voidData) -> void* {
			ThreadData* tData = (ThreadData*) voidData;
			resultprocessingThread(tData->mbfClassInfos, *tData->context, *tData->finalResultPtr, tData->validationBuffer);
			pthread_exit(nullptr);
			return nullptr;
		});
	}

	std::cout << "\033[32m[Result Processor] Result processor finished.\033[39m\n" << std::flush;

	for(size_t i = 0; i < VALIDATION_BUFFER_SIZE(Variables); i++) {
		datas[0].validationBuffer[i].dualBetaSum += datas[1].validationBuffer[i].dualBetaSum;
	}

	numa_free_T(datas[1].validationBuffer, VALIDATION_BUFFER_SIZE(Variables));

	result.validationBuffer = datas[0].validationBuffer;

	return result;
}
