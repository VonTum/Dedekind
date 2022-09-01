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

constexpr int MAX_VALIDATOR_COUNT = 16;
constexpr size_t NUM_VALIDATOR_THREADS_PER_COMPLEX = 8;

// does the necessary math with annotated number of bits, no overflows possible for D(9). 
static BetaSum produceBetaTerm(ClassInfo info, uint64_t pcoeffSum, uint64_t pcoeffCount) {
	// the multiply is max log2(2^35 * 5040 * 5040) = 59.5984160368 bits long, fits in 64 bits
	// Compiler optimizes the divide by compile-time constant VAR_FACTORIAL to an imul, much faster!
	uint64_t deduplicatedTotalPCoeffSum = pcoeffSum * info.classSize; 
	u128 betaTerm = umul128(deduplicatedTotalPCoeffSum, info.intervalSizeDown); // no overflow data loss 64x64->128 bits

	// validation data
	uint64_t deduplicatedCountedPermutes = pcoeffCount * info.classSize;

	return BetaSum{betaTerm, deduplicatedCountedPermutes};
}

static BetaSum produceBetaTerm(ClassInfo info, ProcessedPCoeffSum processedPCoeff) {
	uint64_t pcoeffSum = getPCoeffSum(processedPCoeff);
	uint64_t pcoeffCount = getPCoeffCount(processedPCoeff);
	return produceBetaTerm(info, pcoeffSum, pcoeffCount);
}

static BetaSum sumOverBetas(const ClassInfo* mbfClassInfos, const NodeIndex* idxBuf, const NodeIndex* bufEnd, const ProcessedPCoeffSum* countConnectedSumBuf) {
	BetaSum total = BetaSum{0,0};

	for(const NodeIndex* cur = idxBuf; cur != bufEnd; cur++) {
		ClassInfo info = mbfClassInfos[*cur];

		ProcessedPCoeffSum processedPCoeff = *countConnectedSumBuf++;

		if((processedPCoeff & 0x8000000000000000) != uint64_t(0)) {
			std::cerr << "ECC ERROR DETECTED! At bot Index " << (cur - idxBuf) << ", value was: " << processedPCoeff << std::endl;
			throw "ECC ERROR DETECTED!";
		}

		total += produceBetaTerm(info, processedPCoeff);
	}
	return total;
}

static BetaSum sumOverBetasWithValidationBuffer(const ClassInfo* mbfClassInfos, const NodeIndex* idxBuf, const NodeIndex* bufEnd, const ProcessedPCoeffSum* countConnectedSumBuf, ClassInfo topClassInfo, ValidationData* validationData) {
	BetaSum total = BetaSum{0,0};

	for(const NodeIndex* cur = idxBuf; cur != bufEnd; cur++) {
		ClassInfo info = mbfClassInfos[*cur];

		ProcessedPCoeffSum processedPCoeff = *countConnectedSumBuf++;

		if((processedPCoeff & 0x8000000000000000) != uint64_t(0)) {
			std::cerr << "ECC ERROR DETECTED! At bot Index " << (cur - idxBuf) << ", value was: " << processedPCoeff << std::endl;
			throw "ECC ERROR DETECTED!";
		}

		total += produceBetaTerm(info, processedPCoeff);
		
		BetaSum dualBeta = produceBetaTerm(topClassInfo, processedPCoeff);

		validationData[*cur].dualBetaSum += dualBeta;
	}
	return total;
}

struct DeduplicatedResult {
	BetaSum bufferSum;
	BetaSum dualResult;

	BetaSum merge(unsigned int Variables) const;
};

BetaSum DeduplicatedResult::merge(unsigned int Variables) const {
	return (bufferSum + bufferSum + dualResult) / factorial(Variables);
}

static BetaSum produceBetaResult(unsigned int Variables, const ClassInfo* mbfClassInfos, const JobInfo& curJob, const ProcessedPCoeffSum* pcoeffSumBuf) {
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

static DeduplicatedResult produceBetaResultWithValidation(unsigned int Variables, const ClassInfo* mbfClassInfos, const JobInfo& curJob, const ProcessedPCoeffSum* pcoeffSumBuf, ValidationData* validationData, std::mutex& validationMutex) {
	// Skip the first elements, as it is the top

	ClassInfo info = mbfClassInfos[curJob.bufStart[TOP_DUAL_INDEX]];

	DeduplicatedResult result;

	validationMutex.lock();
	result.bufferSum = sumOverBetasWithValidationBuffer(mbfClassInfos, curJob.bufStart + BUF_BOTTOM_OFFSET, curJob.end(), pcoeffSumBuf + BUF_BOTTOM_OFFSET, info, validationData);
	validationMutex.unlock();

	ProcessedPCoeffSum nonDuplicateTopDual = pcoeffSumBuf[TOP_DUAL_INDEX]; // Index of dual

	result.dualResult = produceBetaTerm(info, getPCoeffSum(nonDuplicateTopDual), getPCoeffCount(nonDuplicateTopDual));

	return result;
}

const ClassInfo* loadClassInfos(unsigned int Variables) {
	return readFlatBuffer<ClassInfo>(FileName::flatClassInfo(Variables), mbfCounts[Variables]);
}

void resultprocessingThread(
	unsigned int Variables,
	const ClassInfo* mbfClassInfos,
	PCoeffProcessingContext& context,
	std::atomic<BetaResult*>& resultPtr,
	void(*validator)(const OutputBuffer&, const void*, ThreadPool&),
	const void* validatorData,
	ValidationData* validationData,
	std::mutex& validationDataMutex
) {
	std::cout << "\033[32m[Result Processor] Result processor Thread started.\033[39m\n" << std::flush;
	ThreadPool pool(NUM_VALIDATOR_THREADS_PER_COMPLEX);
	for(std::optional<OutputBuffer> outputBuffer; (outputBuffer = context.outputQueue.pop_wait()).has_value(); ) {
		OutputBuffer outBuf = outputBuffer.value();

		validator(outBuf, validatorData, pool);

		BetaResult curBetaResult;
		//if constexpr(Variables == 7) std::cout << "Results for job " << outBuf.originalInputData.getTop() << std::endl;
		curBetaResult.topIndex = outBuf.originalInputData.getTop();
		DeduplicatedResult dedupResult = produceBetaResultWithValidation(Variables, mbfClassInfos, outBuf.originalInputData, outBuf.outputBuf, validationData, validationDataMutex);
		curBetaResult.betaSum = dedupResult.merge(Variables);

		context.inputBufferAllocator.free(std::move(outBuf.originalInputData.bufStart));
		context.outputBufferReturnQueue.push(outBuf.outputBuf);

		BetaResult* allocatedSlot = resultPtr.fetch_add(1, std::memory_order_relaxed);
		*allocatedSlot = std::move(curBetaResult);
	}
	std::cout << "\033[32m[Result Processor] Result processor Thread finished.\033[39m\n" << std::flush;
}

ResultProcessorOutput resultProcessor(
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

	result.validationBuffer = new ValidationData[VALIDATION_BUFFER_SIZE(Variables)];
	std::mutex validationMutex;
	resultprocessingThread(Variables, mbfClassInfos, context, finalResultPtr, [](const OutputBuffer&, const void*, ThreadPool&){}, nullptr, result.validationBuffer, validationMutex);

	std::cout << "\033[32m[Result Processor] Result processor finished.\033[39m\n" << std::flush;
	return result;
}

ResultProcessorOutput NUMAResultProcessorWithValidator(
	unsigned int Variables,
	PCoeffProcessingContext& context,
	size_t numResults,
	size_t numValidators,
	void(*validator)(const OutputBuffer&, const void*, ThreadPool&),
	const void* mbfs[2]
) {
	std::cout << "\033[32m[Result Processor] Started loading ClassInfos...\033[39m\n" << std::flush;
	const ClassInfo* mbfClassInfos = loadClassInfos(Variables);
	std::cout << "\033[32m[Result Processor] Finished Loading ClassInfos. Result processor started.\033[39m\n" << std::flush;

	std::cout << "\033[32m[Result Processor] Started loading ClassInfos...\033[39m\n" << std::flush;

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
		unsigned int Variables;
		PCoeffProcessingContext* context;
		const ClassInfo* mbfClassInfos;
		std::atomic<BetaResult*>* finalResultPtr;
		void(*validator)(const OutputBuffer&, const void*, ThreadPool&);
		const void* mbfs;
		ValidationData* validationBuffer;
		std::mutex validationBufferMutex;
	};
	ThreadData datas[2];
	for(int i = 0; i < 2; i++) {
		datas[i].Variables = Variables;
		datas[i].context = &context;
		datas[i].mbfClassInfos = mbfClassInfos;
		datas[i].finalResultPtr = &finalResultPtr;
		datas[i].validator = validator;
		datas[i].mbfs = mbfs[i];
		datas[i].validationBuffer = static_cast<ValidationData*>(validationBuffers[i]);
	}

	auto threadFunc = [](void* voidData) -> void* {
		ThreadData* tData = (ThreadData*) voidData;
		resultprocessingThread(tData->Variables, tData->mbfClassInfos, *tData->context, *tData->finalResultPtr, tData->validator, tData->mbfs, tData->validationBuffer, tData->validationBufferMutex);
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

	for(size_t i = 0; i < VALIDATION_BUFFER_SIZE(Variables); i++) {
		datas[0].validationBuffer[i].dualBetaSum += datas[1].validationBuffer[i].dualBetaSum;
	}

	numa_free_T(datas[1].validationBuffer, VALIDATION_BUFFER_SIZE(Variables));

	result.validationBuffer = datas[0].validationBuffer;

	return result;
}





ValidationBuffer::ValidationBuffer(size_t numElems, const char* numaInterleave) : dualBetas(numa_alloc_interleaved_T<BetaSum>(numElems, numaInterleave), numElems) {}
	
void ValidationBuffer::addValidationData(const OutputBuffer& outBuf, ClassInfo topInfo) {
	uint64_t intervalSizeDown = topInfo.intervalSizeDown;
	uint64_t classSize = topInfo.classSize;
	this->mutex.lock();
	ProcessedPCoeffSum* curPCoeffResult = outBuf.outputBuf;
	for(NodeIndex* curBot = outBuf.originalInputData.bufStart + BUF_BOTTOM_OFFSET; curBot != outBuf.originalInputData.bufEnd; curBot++) {
		BetaSum dualBeta = produceBetaTerm(topInfo, *curPCoeffResult);

		curPCoeffResult++;
	}
	this->mutex.unlock();
}


