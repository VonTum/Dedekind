#pragma once

#include "pcoeffClasses.h"
#include "threadPool.h"
#include "funcTypes.h"
#include "knownData.h"

#include <atomic>
#include <random>
#include <chrono>

constexpr size_t VALIDATE_BEGIN_SIZE = 128;
constexpr size_t VALIDATE_END_SIZE = 128;
constexpr size_t VALIDATE_CHECK_WHOLE_BUFFER_TRESHOLD = 1024; // Should be larger than VALIDATE_BEGIN_SIZE + VALIDATE_END_SIZE
constexpr size_t VALIDATE_RANDOM_BLOCK_SIZE = 32;
constexpr size_t VALIDATE_FRACTION = 10000; // check factorial(Variables)/(VALIDATE_FRACTION)

void validatorStartMessage(NodeIndex topIdx, size_t numBottoms);
void validatorFinishMessage(NodeIndex topIdx, size_t numBottoms, size_t numTestedPCoeffs, std::chrono::time_point<std::chrono::high_resolution_clock> startTime);
void printBadPCoeffSumError(NodeIndex botIdx, size_t elementIdx, ProcessedPCoeffSum foundPCoeffSum, ProcessedPCoeffSum correctPCoeffSum);

template<unsigned int Variables>
uint64_t validateBufferSlice(const OutputBuffer& resultBuf, size_t numToCheck, size_t startAt, Monotonic<Variables> top, const Monotonic<Variables>* mbfs) {
	bool success = true;
	uint64_t totalPCoeffCount = 0;
	for(size_t i = 0; i < numToCheck; i++) {
		size_t elementIndex = startAt + i;
		NodeIndex botIdx = resultBuf.originalInputData.bufStart[elementIndex];
		ProcessedPCoeffSum foundPCoeffSum = resultBuf.outputBuf[elementIndex];
		Monotonic<Variables> bot = mbfs[botIdx];
		ProcessedPCoeffSum correctPCoeffSum = processPCoeffSum(top, bot);

		if(foundPCoeffSum != correctPCoeffSum) {
			printBadPCoeffSumError(botIdx, elementIndex, foundPCoeffSum, correctPCoeffSum);
			success = false;
		}
		totalPCoeffCount += getPCoeffCount(correctPCoeffSum);
	}
	if(!success) {
		exit(-1);
	}
	return totalPCoeffCount;
}

void NO_VALIDATOR(const OutputBuffer& resultBuf, const void* voidMBFs, ThreadPool& pool);

template<unsigned int Variables>
void threadPoolBufferValidator(const OutputBuffer& resultBuf, const void* voidMBFs, ThreadPool& pool) {
	const Monotonic<Variables>* mbfs = static_cast<const Monotonic<Variables>*>(voidMBFs);
	NodeIndex topIdx = resultBuf.originalInputData.getTop();
	size_t numBottoms = resultBuf.originalInputData.getNumberOfBottoms();
	std::chrono::time_point<std::chrono::high_resolution_clock> startTime = std::chrono::high_resolution_clock::now();
	//validatorStartMessage(topIdx, numBottoms);
	Monotonic<Variables> top = mbfs[topIdx];
	size_t numValidated;
	if(numBottoms <= VALIDATE_CHECK_WHOLE_BUFFER_TRESHOLD) {
		numValidated = validateBufferSlice<Variables>(resultBuf, numBottoms, 2, top, mbfs);
	} else {
		size_t pcoeffCountTarget = numBottoms * factorial(Variables) / VALIDATE_FRACTION;
		std::atomic<size_t> totalPCoeffCount;
		totalPCoeffCount.store(0);
		pool.doInParallel([&](){
			std::default_random_engine generator;
			std::uniform_int_distribution<size_t> indexDistribution(2+VALIDATE_BEGIN_SIZE, numBottoms - VALIDATE_END_SIZE - VALIDATE_RANDOM_BLOCK_SIZE);

			while(totalPCoeffCount.fetch_add(validateBufferSlice<Variables>(resultBuf, VALIDATE_RANDOM_BLOCK_SIZE, indexDistribution(generator), top, mbfs)) < pcoeffCountTarget);
		}, [&](){
			size_t beginEndPCoeffCount = 0;
			size_t beginPCoeffCount = validateBufferSlice<Variables>(resultBuf, VALIDATE_BEGIN_SIZE, 2, top, mbfs);
			size_t endPCoeffCount = validateBufferSlice<Variables>(resultBuf, VALIDATE_END_SIZE, numBottoms + 2 - VALIDATE_END_SIZE, top, mbfs);

			totalPCoeffCount.fetch_add(beginPCoeffCount + endPCoeffCount);
		});
		numValidated = totalPCoeffCount.load();
	}
	validatorFinishMessage(topIdx, numBottoms, numValidated, startTime);
}
