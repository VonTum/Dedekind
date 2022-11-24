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
#include <immintrin.h>

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
static BetaSum sumOverBetas(const ClassInfo* mbfClassInfos, const OutputBuffer& buf, const std::function<void(const OutputBuffer&, const char*)>& errorBufFunc) {
	BetaSum total = BetaSum{0,0};

	ProcessedPCoeffSum* countConnectedSumBuf = buf.outputBuf + BUF_BOTTOM_OFFSET;
	NodeIndex* bufStart = buf.originalInputData.bufStart + BUF_BOTTOM_OFFSET;
	NodeIndex* bufEnd = buf.originalInputData.bufEnd;
	for(const NodeIndex* cur = bufStart; cur != bufEnd; cur++) {
		ClassInfo info = mbfClassInfos[*cur];

		ProcessedPCoeffSum processedPCoeff = *countConnectedSumBuf++;

		if((processedPCoeff & 0x8000000000000000) != uint64_t(0)) {
			std::cerr << "ECC ERROR DETECTED! At bot Index " << (cur - bufStart) << ", value was: " << processedPCoeff << std::endl;
			errorBufFunc(buf, "ecc");
		}

		total += produceBetaTerm(info, processedPCoeff);
	}
	return total;
}

// Takes validation buffer
static BetaSum sumOverBetas(const ClassInfo* mbfClassInfos, const OutputBuffer& buf, const std::function<void(const OutputBuffer&, const char*)>& errorBufFunc, ClassInfo topDualClassInfo, ValidationData* validationBuf) {
	BetaSum total = BetaSum{0,0};

	ProcessedPCoeffSum* countConnectedSumBuf = buf.outputBuf + BUF_BOTTOM_OFFSET;
	NodeIndex* bufStart = buf.originalInputData.bufStart + BUF_BOTTOM_OFFSET;
	NodeIndex* bufEnd = buf.originalInputData.bufEnd;
	for(const NodeIndex* cur = bufStart; cur != bufEnd; cur++) {
		ClassInfo info = mbfClassInfos[*cur];

		ProcessedPCoeffSum processedPCoeff = *countConnectedSumBuf++;

		if((processedPCoeff & 0x8000000000000000) != uint64_t(0)) {
			std::cerr << "ECC ERROR DETECTED! At bot Index " << (cur - bufStart) << ", value was: " << processedPCoeff << std::endl;
			errorBufFunc(buf, "ecc");
		}

		total += produceBetaTerm(info, processedPCoeff);

		// This is the sum to be added to the top "inv(bot)", because we deduplicated it off from that top
		validationBuf[*cur].dualBetaSum += produceBetaTerm(topDualClassInfo, processedPCoeff);
	}
	return total;
}

// Optionally takes a buffer for validation data
static BetaSumPair produceBetaResult(const ClassInfo* mbfClassInfos, const OutputBuffer& buf, const std::function<void(const OutputBuffer&, const char*)>& errorBufFunc, ValidationData* validationBuf = nullptr) {
	// Skip the first elements, as it is the top
	BetaSumPair result;
	if(validationBuf != nullptr) {
		NodeIndex topDualIdx = buf.originalInputData.bufStart[TOP_DUAL_INDEX];
		ClassInfo topDualClassInfo = mbfClassInfos[topDualIdx];	

		result.betaSum = sumOverBetas(mbfClassInfos, buf, errorBufFunc, topDualClassInfo, validationBuf);
	} else {
		result.betaSum = sumOverBetas(mbfClassInfos, buf, errorBufFunc);
	}

#ifdef PCOEFF_DEDUPLICATE
	ProcessedPCoeffSum nonDuplicateTopDual = buf.outputBuf[TOP_DUAL_INDEX]; // Index of dual

	ClassInfo info = mbfClassInfos[buf.originalInputData.bufStart[TOP_DUAL_INDEX]];

	result.betaSumDualDedup = produceBetaTerm(info, getPCoeffSum(nonDuplicateTopDual), getPCoeffCount(nonDuplicateTopDual));
#else
	result.betaSumDualDedup = BetaSum{0, 0};
#endif
	return result;
}

static void resultprocessingThread(
	const ClassInfo* mbfClassInfos,
	PCoeffProcessingContextEighth& context,
	std::atomic<BetaResult*>& resultPtr,
	ValidationData* validationData,
	const std::function<void(const OutputBuffer&, const char*)>& errorBufFunc
) {
	std::cout << "\033[32m[Result Processor] Result processor Thread started.\033[39m\n" << std::flush;
	for(std::optional<OutputBuffer> outputBuffer; (outputBuffer = context.outputQueue.pop_wait()).has_value(); ) {
		OutputBuffer buf = outputBuffer.value();

		BetaResult curBetaResult;
		//if constexpr(Variables == 7) std::cout << "Results for job " << buf.originalInputData.getTop() << std::endl;
		curBetaResult.topIndex = buf.originalInputData.getTop();

		curBetaResult.dataForThisTop = produceBetaResult(mbfClassInfos, buf, errorBufFunc, validationData);

		context.validationQueue.push(buf);

		BetaResult* allocatedSlot = resultPtr.fetch_add(1);
		*allocatedSlot = std::move(curBetaResult);
	}
	context.validationQueue.close();
	std::cout << "\033[32m[Result Processor] Result processor Thread finished.\033[39m\n" << std::flush;
}

ResultProcessorOutput NUMAResultProcessor(
	unsigned int Variables,
	PCoeffProcessingContext& context,
	const std::function<void(const OutputBuffer&, const char*)>& errorBufFunc
) {
	void* numaClassInfos[2];
	size_t classInfoBufferSize = mbfCounts[Variables] * sizeof(ClassInfo);
	allocSocketBuffers(classInfoBufferSize, numaClassInfos);
	readFlatVoidBufferNoMMAP(FileName::flatClassInfo(Variables), classInfoBufferSize, numaClassInfos[0]);
	memcpy(numaClassInfos[1], numaClassInfos[0], classInfoBufferSize);
	std::cout << "\033[32m[Result Processor] Finished Loading ClassInfos. Allocating validation buffers\033[39m\n" << std::flush;

	ResultProcessorOutput result;
	result.results.resize(context.tops.size());
	std::atomic<BetaResult*> finalResultPtr;
	finalResultPtr.store(&result.results[0]);

	void* validationBuffers[8];
	size_t validationBufferSize = sizeof(ValidationData) * VALIDATION_BUFFER_SIZE(Variables);
	allocNumaNodeBuffers(validationBufferSize, validationBuffers);
	std::cout << "\033[32m[Result Processor] Allocated validation buffers. Starting result processing threads\033[39m\n" << std::flush;

	struct ThreadData {
		size_t validationBufferSize;
		PCoeffProcessingContextEighth* context;
		const ClassInfo* mbfClassInfos;
		std::atomic<BetaResult*>* finalResultPtr;
		ValidationData* validationBuffer;
		int numaNode;
		unsigned int Variables;
		const std::function<void(const OutputBuffer&, const char*)>* errorBufFunc;
	};
	ThreadData datas[8];
	for(int i = 0; i < 8; i++) {
		datas[i].validationBufferSize = validationBufferSize;
		datas[i].context = context.numaQueues[i / 4].ptr;
		datas[i].mbfClassInfos = static_cast<const ClassInfo*>(numaClassInfos[i / 4]);
		datas[i].finalResultPtr = &finalResultPtr;
		datas[i].validationBuffer = static_cast<ValidationData*>(validationBuffers[i]);
		datas[i].numaNode = i;
		datas[i].Variables = Variables;
		datas[i].errorBufFunc = &errorBufFunc;
	}

	PThreadBundle threads = spreadThreads(8, CPUAffinityType::NUMA_DOMAIN, datas, [](void* voidData) -> void* {
		ThreadData* tData = (ThreadData*) voidData;
		setThreadName(("Result " + std::to_string(tData->numaNode)).c_str());
		memset(static_cast<void*>(tData->validationBuffer), 0, tData->validationBufferSize);
		resultprocessingThread(tData->mbfClassInfos, *tData->context, *tData->finalResultPtr, tData->validationBuffer, *tData->errorBufFunc);
		pthread_exit(nullptr);
		return nullptr;
	});
	threads.join();

	std::cout << "\033[32m[Result Processor] Result processor finished.\033[39m\n" << std::flush;

	for(size_t i = 0; i < VALIDATION_BUFFER_SIZE(Variables); i++) {
		for(int otherNode = 1; otherNode < 8; otherNode++) {
			datas[0].validationBuffer[i].dualBetaSum += datas[otherNode].validationBuffer[i].dualBetaSum;
		}
	}

	for(int i = 0; i < 2; i++) {
		numa_free(numaClassInfos[i], classInfoBufferSize);
	}
	for(int otherNode = 1; otherNode < 8; otherNode++) {
		numa_free(validationBuffers[otherNode], validationBufferSize);
	}

	result.validationBuffer = datas[0].validationBuffer;

	return result;
}

