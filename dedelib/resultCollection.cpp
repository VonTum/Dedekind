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

#define SECOND_RUN

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

static ProcessedPCoeffSum* checkBuffer(ProcessedPCoeffSum* buf, ProcessedPCoeffSum* bufEnd) {
	while(buf != bufEnd) {
		if(__builtin_expect((*buf >> 61) != uint64_t(0), 0)) {
			return buf;
		}
		buf++;
	}
	return nullptr;
}

static BetaSum sumOverBetas(const ClassInfo* mbfClassInfos, const OutputBuffer& buf, bool& failed) {
	BetaSum total = BetaSum{0,0};

	ProcessedPCoeffSum* countConnectedSumBuf = buf.outputBuf + BUF_BOTTOM_OFFSET;
	NodeIndex* bufStart = buf.originalInputData.bufStart + BUF_BOTTOM_OFFSET;
	NodeIndex* bufEnd = buf.originalInputData.bufEnd;
	for(const NodeIndex* cur = bufStart; cur != bufEnd; cur++) {
		ClassInfo info = mbfClassInfos[*cur];

		ProcessedPCoeffSum processedPCoeff = *countConnectedSumBuf++;

		if(__builtin_expect((processedPCoeff >> 61) != uint64_t(0), 0)) {
			failed = true;
			return total;
		}

		total += produceBetaTerm(info, processedPCoeff);
	}

	return total;
}

static void addValidationData(const OutputBuffer& buf, ClassInfo topDualClassInfo, ValidationData* validationBuf) {
	ProcessedPCoeffSum* countConnectedSumBuf = buf.outputBuf + BUF_BOTTOM_OFFSET;
	NodeIndex* bufStart = buf.originalInputData.bufStart + BUF_BOTTOM_OFFSET;
	NodeIndex* bufEnd = buf.originalInputData.bufEnd;
	for(const NodeIndex* cur = bufStart; cur != bufEnd; cur++) {
		ProcessedPCoeffSum processedPCoeff = *countConnectedSumBuf++;

		// This is the sum to be added to the top "inv(bot)", because we deduplicated it off from that top
		validationBuf[*cur].dualBetaSum += produceBetaTerm(topDualClassInfo, processedPCoeff);
	}
}

static BetaSumPair produceBetaResult(const ClassInfo* mbfClassInfos, const OutputBuffer& buf, bool& failed) {
	BetaSumPair result;

	result.betaSum = sumOverBetas(mbfClassInfos, buf, failed);

#ifdef PCOEFF_DEDUPLICATE
	ProcessedPCoeffSum nonDuplicateTopDual = buf.outputBuf[TOP_DUAL_INDEX]; // Index of dual

	ClassInfo info = mbfClassInfos[buf.originalInputData.bufStart[TOP_DUAL_INDEX]];

	result.betaSumDualDedup = produceBetaTerm(info, getPCoeffSum(nonDuplicateTopDual), getPCoeffCount(nonDuplicateTopDual));
#else
	result.betaSumDualDedup = BetaSum{0, 0};
#endif
	return result;
}


struct ResultProcessingThreadData {
	size_t validationBufferSize;
	PCoeffProcessingContext* context;
	const ClassInfo* mbfClassInfos;
	std::atomic<BetaResult*>* finalResultPtr;
	ValidationData* validationBuffer;
	int numaNode;
	unsigned int Variables;
	const std::function<void(const OutputBuffer&, const char*, bool)>* errorBufFunc;

#ifdef SECOND_RUN
	const u128* firstRunBetaSums;
#endif
};
static void* resultprocessingPThread(void* voidData) {
	ResultProcessingThreadData* tData = (ResultProcessingThreadData*) voidData;
	setThreadName(("Result " + std::to_string(tData->numaNode)).c_str());
	memset(static_cast<void*>(tData->validationBuffer), 0, tData->validationBufferSize);
	//resultprocessingThread(tData->mbfClassInfos, *tData->context, *tData->finalResultPtr, tData->validationBuffer, *tData->errorBufFunc);

	const ClassInfo* mbfClassInfos = tData->mbfClassInfos;
	ValidationData* validationBuffer = tData->validationBuffer;
	const std::function<void(const OutputBuffer&, const char*, bool)>& errorBufFunc = *tData->errorBufFunc;
	PCoeffProcessingContextEighth& subContext = *tData->context->numaQueues[tData->numaNode / 4];
	std::atomic<BetaResult*>& finalResultPtr = *tData->finalResultPtr;
	std::cout << "\033[32m[Result Processor] Result processor Thread started.\033[39m\n" << std::flush;
	for(std::optional<OutputBuffer> outputBuffer; (outputBuffer = subContext.outputQueue.pop_wait()).has_value(); ) {
		OutputBuffer buf = outputBuffer.value();

		BetaResult curBetaResult;
		//if constexpr(Variables == 7) std::cout << "Results for job " << buf.originalInputData.getTop() << std::endl;
		curBetaResult.topIndex = buf.originalInputData.getTop();

#ifndef SECOND_RUN
		ProcessedPCoeffSum* errorLocation = checkBuffer(buf.outputBuf + 2, buf.outputBuf + buf.originalInputData.bufferSize());
		if(errorLocation != nullptr) {
			ProcessedPCoeffSum errorValue = *errorLocation;
			const char* errorMessage = "ECC ERROR DETECTED! At bot Index ";
			const char* errCat = "ecc";
			bool recoverable = false;
			if(errorValue == 0xFFFFFFFFFFFFFFFF) {
				errorMessage = "MISSING DATA ERROR DETECTED! At bot Index ";
				errCat = "missingData";
				recoverable = true;
			} else if(errorValue == 0xEEEEEEEEEEEEEEEE) {
				errorMessage = "MISSING IN FPGA DATA ERROR DETECTED! At bot Index ";
				errCat = "missingInFPGA";
				recoverable = true;
			}
			std::cerr << errorMessage + std::to_string(errorLocation - buf.outputBuf) + ", value was: " + std::to_string(errorValue) + "\n" << std::flush;
			errorBufFunc(buf, errCat, recoverable);

			if(recoverable) {
				std::cout << "\033[32m[Result Processor] Retrying buffer for top " + std::to_string(buf.originalInputData.getTop()) + "!\033[39m\n" << std::flush;
				tData->context->inputQueue.push(tData->numaNode / 4, buf.originalInputData); // Return the buffer to try again
				subContext.freeBuf(buf.outputBuf, buf.originalInputData.alignedBufferSize());
				continue;
			}
		}
#endif

		bool failed = false;
		curBetaResult.dataForThisTop = produceBetaResult(mbfClassInfos, buf, failed);

#ifdef SECOND_RUN
		//if(curBetaResult.dataForThisTop.betaSum.betaSum != tData->)
#endif
		if(!failed) {
#ifdef PCOEFF_DEDUPLICATE
			NodeIndex topDualIdx = buf.originalInputData.bufStart[TOP_DUAL_INDEX];
			ClassInfo topDualClassInfo = mbfClassInfos[topDualIdx];	
			addValidationData(buf, topDualClassInfo, validationBuffer);
#endif

			subContext.validationQueue.push(buf);
			BetaResult* allocatedSlot = finalResultPtr.fetch_add(1);
			*allocatedSlot = std::move(curBetaResult);
		}
	}
	std::cout << "\033[32m[Result Processor] Result processor Thread finished.\033[39m\n" << std::flush;

	pthread_exit(nullptr);
	return nullptr;
}

ResultProcessorOutput NUMAResultProcessor(
	unsigned int Variables,
	PCoeffProcessingContext& context,
	const std::function<void(const OutputBuffer&, const char*, bool)>& errorBufFunc
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

#ifdef SECOND_RUN
	const u128* firstRunBetaSums = readFlatBuffer<u128>(FileName::firstRunBetaSums(Variables), mbfCounts[Variables]);
#endif

	ResultProcessingThreadData datas[8];
	for(int i = 0; i < 8; i++) {
		datas[i].validationBufferSize = validationBufferSize;
		datas[i].context = &context;
		datas[i].mbfClassInfos = static_cast<const ClassInfo*>(numaClassInfos[i / 4]);
		datas[i].finalResultPtr = &finalResultPtr;
		datas[i].validationBuffer = static_cast<ValidationData*>(validationBuffers[i]);
		datas[i].numaNode = i;
		datas[i].Variables = Variables;
		datas[i].errorBufFunc = &errorBufFunc;
#ifdef SECOND_RUN
		datas[i].firstRunBetaSums = firstRunBetaSums;
#endif
	}

	PThreadBundle threads = spreadThreads(8, CPUAffinityType::NUMA_DOMAIN, datas, resultprocessingPThread);
	threads.join();

	for(size_t i = 0; i < NUMA_SLICE_COUNT; i++) {
		context.numaQueues[i]->validationQueue.close();
	}

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

