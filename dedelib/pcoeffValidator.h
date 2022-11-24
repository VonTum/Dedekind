#pragma once

#include "pcoeffClasses.h"
#include "threadPool.h"
#include "funcTypes.h"
#include "knownData.h"
#include "processingContext.h"

#include <atomic>
#include <random>
#include <chrono>
#include <immintrin.h>

constexpr size_t VALIDATE_BEGIN_SIZE = 64;
constexpr size_t VALIDATE_END_SIZE = 16;
constexpr size_t VALIDATE_CHECK_WHOLE_BUFFER_TRESHOLD = 128;
static_assert(VALIDATE_CHECK_WHOLE_BUFFER_TRESHOLD >= VALIDATE_BEGIN_SIZE + VALIDATE_END_SIZE);
constexpr size_t VALIDATE_RANDOM_BLOCK_SIZE = 2;
constexpr int VALIDATE_WORKER_COUNT = 5;
constexpr uint64_t VALIDATE_FRACTION = 120000; // check at least factorial(Variables)/(VALIDATE_FRACTION)

void validatorOnlineMessage(int validatorIdx);
void validatorExitMessage(int validatorIdx);
void validatorStartMessage(int validatorIdx, NodeIndex topIdx, size_t numBottoms);
void validatorFinishMessage(int validatorIdx, NodeIndex topIdx, size_t numBottoms, size_t numTestedPCoeffs, std::chrono::time_point<std::chrono::high_resolution_clock> startTime);
void printBadPCoeffSumError(NodeIndex botIdx, size_t elementIdx, ProcessedPCoeffSum foundPCoeffSum, ProcessedPCoeffSum correctPCoeffSum);

template<unsigned int Variables>
uint64_t validateBufferSlice(const NodeIndex* indices, const ProcessedPCoeffSum* results, size_t numToCheck, size_t startAt, Monotonic<Variables> top, const Monotonic<Variables>* mbfs, std::atomic<bool>& correct) {
	uint64_t totalPCoeffCount = 0;
	for(size_t i = 0; i < numToCheck; i++) {
		size_t elementIndex = startAt + i;
		NodeIndex botIdx = indices[elementIndex];
		ProcessedPCoeffSum foundPCoeffSum = results[elementIndex];
		_mm_prefetch(&mbfs[botIdx], _MM_HINT_NTA); // Try to reduce cache pollution
		Monotonic<Variables> bot = mbfs[botIdx];
		ProcessedPCoeffSum correctPCoeffSum = processPCoeffSum(top, bot);

		if(foundPCoeffSum != correctPCoeffSum) {
			printBadPCoeffSumError(botIdx, elementIndex, foundPCoeffSum, correctPCoeffSum);
			correct.store(false);
		}
		totalPCoeffCount += getPCoeffCount(correctPCoeffSum);
	}
	return totalPCoeffCount;
}

constexpr int VALIDATOR_EXIT = -1;

struct ValidatorWorkerData {
	alignas(64) OutputBuffer jobs[2];
	alignas(64) std::atomic<uint64_t> processedCounts[2];

	/* These next two atomics fully control the synchronization between all threads

		Flipping to a new job happens as follows:
			First the main thread waits for all worker threads to leave the job to be replaced. 
				Wait for numProcessingB to all flip to the other job (numProcessingB == 0 for B, numProcessingB == threadCount for A)
			Main thread initiates a shift by flipping nextJob to its new job
			Worker threads switch to new job as they see the changed nextJob. 
				On change, they increment or decrement numProcessingB
		
		Threads wake up processing A, so we must wait for them to all switch to B before overwriting A, 
		can't just check the number 'still working on A' as threads that hadn't woken up yet won't be counted
			
	*/
	alignas(64) 
	std::atomic<int> nextJob; // Controlled by main thread. Either A=0, or B=1, or VALIDATOR_EXIT
	std::atomic<int> numProcessingB; // Controlled by worker threads. numProcessingA = totalThreads - numProcessingB
	std::atomic<bool> correct[2];
	alignas(64)
	const void* mbfs;
	std::chrono::time_point<std::chrono::high_resolution_clock> startTimes[2];


	ValidatorWorkerData(const void* mbfs) : 
		mbfs(mbfs) {
		numProcessingB.store(0); // All threads start by processing A
	}

	void setNewJob(OutputBuffer&& newBuf, int jobIdx) {
		jobs[jobIdx] = std::move(newBuf);
		processedCounts[jobIdx].store(0);
		nextJob.store(jobIdx);
		correct[jobIdx].store(true);
	}

	void workerSwitchTo(int newJob) {
		numProcessingB.fetch_add(newJob == 1 ? 1 : -1);
	}
	bool allWorkersLeftJob(int jobI) {
		int foundNumProcessing = numProcessingB.load();
		int targetNumProcessing = (jobI == 1) ? 0 : VALIDATE_WORKER_COUNT;
		return foundNumProcessing == targetNumProcessing;
	}
};

template<unsigned int Variables>
uint64_t validateRandomBufferPart(const NodeIndex* indices, const ProcessedPCoeffSum* results, Monotonic<Variables> top, size_t numBottoms, const Monotonic<Variables>* mbfs, std::default_random_engine& generator, std::atomic<bool>& correct) {
	std::uniform_int_distribution<size_t> indexDistribution(2+VALIDATE_BEGIN_SIZE, numBottoms - VALIDATE_END_SIZE - VALIDATE_RANDOM_BLOCK_SIZE);

	return validateBufferSlice(indices, results, VALIDATE_RANDOM_BLOCK_SIZE, indexDistribution(generator), top, mbfs, correct);
}

template<unsigned int Variables>
void validateRandomBufferPart(ValidatorWorkerData& workerData, int jobI, const Monotonic<Variables>* mbfs, std::default_random_engine& generator, std::atomic<bool>& correct) {
	const OutputBuffer& resultBuf = workerData.jobs[jobI];
	NodeIndex topIdx = resultBuf.originalInputData.getTop();
	size_t numBottoms = resultBuf.originalInputData.getNumberOfBottoms();
	const NodeIndex* indices = resultBuf.originalInputData.bufStart;
	const ProcessedPCoeffSum* results = resultBuf.outputBuf;
	Monotonic<Variables> top = mbfs[topIdx];

	uint64_t processedAmount = ::validateRandomBufferPart(indices, results, top, numBottoms, mbfs, generator, correct);
	workerData.processedCounts[jobI].fetch_add(processedAmount);
}

void freeBuffersAfterValidation(int complexI, PCoeffProcessingContextEighth& context, const OutputBuffer& resultBuf, uint64_t workAmount, std::chrono::time_point<std::chrono::high_resolution_clock>& startTime);
void freeBuffersAfterValidation(int complexI, PCoeffProcessingContextEighth& context, ValidatorWorkerData& workerData, int jobToFree);

template<unsigned int Variables>
bool validateWholeBufferIfTooSmall(int complexI, const std::function<void(const OutputBuffer&, const char*)>& errorBufFunc, PCoeffProcessingContextEighth& context, OutputBuffer& resultBuf, const Monotonic<Variables>* mbfs) {
	size_t numBottoms = resultBuf.originalInputData.getNumberOfBottoms();
	if(numBottoms <= VALIDATE_CHECK_WHOLE_BUFFER_TRESHOLD) {
		std::chrono::time_point<std::chrono::high_resolution_clock> startTime = std::chrono::high_resolution_clock::now();
		NodeIndex topIdx = resultBuf.originalInputData.getTop();
		Monotonic<Variables> top = mbfs[topIdx];

		std::atomic<bool> correct;
		correct.store(true);
		uint64_t numValidated = validateBufferSlice(resultBuf.originalInputData.bufStart, resultBuf.outputBuf, numBottoms, 2, top, mbfs, correct);

		if(correct.load() == false) {
			errorBufFunc(resultBuf, "validator");
		}
		freeBuffersAfterValidation(complexI, context, resultBuf, numValidated, startTime);

		return true;
	} else {
		return false;
	}
}


template<unsigned int Variables>
void* validationWorkerThread(void* voidData) {
	std::default_random_engine generator;
	setThreadName("Valid Worker");

	ValidatorWorkerData* validatorData = (ValidatorWorkerData*) voidData;
	const Monotonic<Variables>* mbfs = static_cast<const Monotonic<Variables>*>(validatorData->mbfs);

	// int curJob = validatorData->nextJob.load(); // THIS WAS A BUG, main thread could already set this to 1 before worker starts, leading to a blocked worker
	int curJob = 0;

	while(curJob != VALIDATOR_EXIT) {
		const OutputBuffer& resultBuf = validatorData->jobs[curJob];
		std::atomic<bool>& correctAtomic = validatorData->correct[curJob];
		NodeIndex topIdx = resultBuf.originalInputData.getTop();
		size_t numBottoms = resultBuf.originalInputData.getNumberOfBottoms();
		Monotonic<Variables> top = mbfs[topIdx];
		const NodeIndex* indices = resultBuf.originalInputData.bufStart;
		const ProcessedPCoeffSum* results = resultBuf.outputBuf;

		int newJob;
		do {
			uint64_t workAmount = validateRandomBufferPart<Variables>(indices, results, top, numBottoms, mbfs, generator, correctAtomic);
			validatorData->processedCounts[curJob].fetch_add(workAmount);
			newJob = validatorData->nextJob.load();
		} while(newJob == curJob);
		curJob = newJob;

		validatorData->workerSwitchTo(curJob);
	}

	pthread_exit(nullptr);
	return nullptr;
}

struct ValidatorThreadData {
	PCoeffProcessingContextEighth* context;
	const void* mbfs;
	const std::function<void(const OutputBuffer&, const char*)>* errorBufFunc;
	int complexI;
};
template<unsigned int Variables>
void* continuousValidatorPThread(void* voidData) {
	ValidatorThreadData* validatorData = (ValidatorThreadData*) voidData;
	int complexI = validatorData->complexI;
	PCoeffProcessingContextEighth& context = *validatorData->context;
	const std::function<void(const OutputBuffer&, const char*)>& errorBufFunc = *validatorData->errorBufFunc;
	std::string threadName = "Validator " + std::to_string(complexI);
	setThreadName(threadName.c_str());
	validatorOnlineMessage(complexI);

	const Monotonic<Variables>* mbfs = static_cast<const Monotonic<Variables>*>(validatorData->mbfs);

	newInitialJobFromShortJob:
	std::optional<OutputBuffer> jobOpt = context.validationQueue.pop_wait();
	if(jobOpt.has_value()) {
		OutputBuffer initalJobBuf = std::move(jobOpt).value();
		if(validateWholeBufferIfTooSmall(complexI, errorBufFunc, context, initalJobBuf, mbfs)) {
			goto newInitialJobFromShortJob;
		}

		std::default_random_engine generator;
		
		ValidatorWorkerData workerData(validatorData->mbfs);
		int curJob = 0;
		bool isFirstBuffer = true;
		workerData.setNewJob(std::move(initalJobBuf), 0);
		PThreadBundle workerThreads = multiThread(VALIDATE_WORKER_COUNT, complexI, CPUAffinityType::COMPLEX, &workerData, validationWorkerThread<Variables>);
		while(true) {
			const OutputBuffer& resultBuf = workerData.jobs[curJob];
			NodeIndex topIdx = resultBuf.originalInputData.getTop();
			size_t numBottoms = resultBuf.originalInputData.getNumberOfBottoms();
			const NodeIndex* indices = resultBuf.originalInputData.bufStart;
			const ProcessedPCoeffSum* results = resultBuf.outputBuf;
			Monotonic<Variables> top = mbfs[topIdx];
			uint64_t minimumWorkTarget = numBottoms * factorial(Variables) / VALIDATE_FRACTION;

			//validatorStartMessage(complexI, topIdx, numBottoms);
			workerData.startTimes[curJob] = std::chrono::high_resolution_clock::now();
			
			uint64_t beginPCoeffCount = validateBufferSlice(indices, results, VALIDATE_BEGIN_SIZE, 2, top, mbfs, workerData.correct[curJob]);
			uint64_t endPCoeffCount = validateBufferSlice(indices, results, VALIDATE_END_SIZE, numBottoms + 2 - VALIDATE_END_SIZE, top, mbfs, workerData.correct[curJob]);

			uint64_t workedNow = beginPCoeffCount + endPCoeffCount;

			while(workerData.processedCounts[curJob].fetch_add(workedNow) < minimumWorkTarget) {
				workedNow = validateRandomBufferPart(indices, results, top, numBottoms, mbfs, generator, workerData.correct[curJob]);
			}

			// Minimum work target met!
			// Clear out space for new buffer. Wait until attempting to acquire new buffer to validate. Continue to validate in the meantime
			if(!isFirstBuffer) {
				int jobToReplace = 1 - curJob;
				while(!workerData.allWorkersLeftJob(jobToReplace)) {
					validateRandomBufferPart(indices, results, top, numBottoms, mbfs, generator, workerData.correct[jobToReplace]);
				}

				if(workerData.correct[jobToReplace].load() == false) {
					errorBufFunc(workerData.jobs[jobToReplace], "validator");
				}
				freeBuffersAfterValidation(complexI, context, workerData, jobToReplace);
			}

			grabNewJobFromShortJob:
			std::optional<OutputBuffer> newJob = context.validationQueue.pop_wait();
			if(!newJob.has_value()) { // Closed
				// Return final buffers
				workerData.nextJob.store(VALIDATOR_EXIT);
				workerThreads.join();

				if(workerData.correct[curJob].load() == false) {
					errorBufFunc(workerData.jobs[curJob], "validator");
				}
				freeBuffersAfterValidation(complexI, context, workerData, curJob);

				goto exit;
			}
			if(validateWholeBufferIfTooSmall(complexI, errorBufFunc, context, newJob.value(), mbfs)) {
				goto grabNewJobFromShortJob;
			}

			curJob = 1 - curJob;
			isFirstBuffer = false;

			workerData.setNewJob(std::move(newJob.value()), curJob);
		}
	}
	
	exit:
	validatorExitMessage(complexI);
	pthread_exit(nullptr);
	return nullptr;
}

template<unsigned int Variables>
bool threadPoolBufferValidator(int complexI, const OutputBuffer& resultBuf, const Monotonic<Variables>* mbfs, ThreadPool& pool) {
	NodeIndex topIdx = resultBuf.originalInputData.getTop();
	size_t numBottoms = resultBuf.originalInputData.getNumberOfBottoms();
	const NodeIndex* indices = resultBuf.originalInputData.bufStart;
	const ProcessedPCoeffSum* results = resultBuf.outputBuf;
	std::chrono::time_point<std::chrono::high_resolution_clock> startTime = std::chrono::high_resolution_clock::now();
	//validatorStartMessage(topIdx, numBottoms);
	Monotonic<Variables> top = mbfs[topIdx];
	size_t numValidated;
	std::atomic<bool> correct;
	correct.store(true);
	if(numBottoms <= VALIDATE_CHECK_WHOLE_BUFFER_TRESHOLD) {
		numValidated = validateBufferSlice(indices, results, numBottoms, 2, top, mbfs, correct);
	} else {
		size_t pcoeffCountTarget = numBottoms * factorial(Variables) / VALIDATE_FRACTION;
		std::atomic<size_t> totalPCoeffCount;
		totalPCoeffCount.store(0);
		pool.doInParallel([&](){
			std::default_random_engine generator;

			while(totalPCoeffCount.fetch_add(validateRandomBufferPart(indices, results, top, numBottoms, mbfs, generator, correct)) < pcoeffCountTarget);
		}, [&](){
			size_t beginPCoeffCount = validateBufferSlice(indices, results, VALIDATE_BEGIN_SIZE, 2, top, mbfs, correct);
			size_t endPCoeffCount = validateBufferSlice(indices, results, VALIDATE_END_SIZE, numBottoms + 2 - VALIDATE_END_SIZE, top, mbfs, correct);

			totalPCoeffCount.fetch_add(beginPCoeffCount + endPCoeffCount);
		});
		numValidated = totalPCoeffCount.load();
	}

	validatorFinishMessage(complexI, topIdx, numBottoms, numValidated, startTime);

	return correct.load();
}

template<unsigned int Variables>
void* basicValidatorPThread(void* voidData) {
	ValidatorThreadData* validatorData = (ValidatorThreadData*) voidData;
	PCoeffProcessingContextEighth& context = *validatorData->context;
	setThreadName(("BasicValidator " + std::to_string(validatorData->complexI)).c_str());
	validatorOnlineMessage(validatorData->complexI);

	const Monotonic<Variables>* mbfs = static_cast<const Monotonic<Variables>*>(validatorData->mbfs);

	ThreadPool pool(VALIDATE_WORKER_COUNT);
	for(std::optional<OutputBuffer> outputBuffer; (outputBuffer = context.validationQueue.pop_wait()).has_value(); ) {
		OutputBuffer outBuf = outputBuffer.value();

		bool wasCorrect = threadPoolBufferValidator(validatorData->complexI, outBuf, mbfs, pool);

		if(!wasCorrect) {
			const std::function<void(const OutputBuffer&, const char*)>& errorBufFunc = *validatorData->errorBufFunc;
			errorBufFunc(outBuf, "validator");
		}

		context.inputBufferAlloc.push(outBuf.originalInputData.bufStart);
		context.resultBufferAlloc.push(outBuf.outputBuf);
	}

	validatorExitMessage(validatorData->complexI);
	pthread_exit(nullptr);
	return nullptr;
}

void* noValidatorPThread(void* voidData);
