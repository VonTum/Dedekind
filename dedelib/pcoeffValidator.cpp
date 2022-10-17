#include "pcoeffValidator.h"

#include <iostream>
#include <string>

void validatorOnlineMessage(int validatorIdx) {
	std::cout << "\033[35m[Validator " + std::to_string(validatorIdx) + "] Started\033[39m\n" << std::flush;
}
void validatorExitMessage(int validatorIdx) {
	std::cout << "\033[35m[Validator " + std::to_string(validatorIdx) + "] Exit\033[39m\n" << std::flush;
}
void validatorStartMessage(int validatorIdx, NodeIndex topIdx, size_t numBottoms) {
	std::cout << "\033[35m[Validator " + std::to_string(validatorIdx) + "] Validating top " + std::to_string(topIdx) + " with " + std::to_string(numBottoms) + " bottoms\033[39m\n" << std::flush;
}
void validatorFinishMessage(int validatorIdx, NodeIndex topIdx, size_t numBottoms, size_t numTestedPCoeffs, std::chrono::time_point<std::chrono::high_resolution_clock> startTime) {
	std::chrono::nanoseconds deltaTime = std::chrono::high_resolution_clock::now() - startTime;
	std::cout << "\033[35m[Validator " + std::to_string(validatorIdx) + "] Correct top " + std::to_string(topIdx) + " with " + std::to_string(numBottoms) + " bottoms: " + std::to_string(numTestedPCoeffs) + " p-coefficients checked in " + std::to_string(deltaTime.count() / 1000000.0) + "ms\033[39m\n" << std::flush;
}

void printBadPCoeffSumError(NodeIndex botIdx, size_t elementIdx, ProcessedPCoeffSum foundPCoeffSum, ProcessedPCoeffSum correctPCoeffSum) {
	std::cerr << "\033[35m[Validator] \033[101mERROR\033[49m at bottom "
		 + std::to_string(elementIdx)
		 + ", Bot=" + std::to_string(elementIdx)
		 + ", true count=" + std::to_string(getPCoeffCount(correctPCoeffSum))
		 + ", found count=" + std::to_string(getPCoeffCount(foundPCoeffSum))
		 + ", true sum=" + std::to_string(getPCoeffSum(correctPCoeffSum))
		 + ", found sum=" + std::to_string(getPCoeffSum(foundPCoeffSum))
		 + "\033[39m\n" << std::flush;
}

void freeBuffersAfterValidation(int complexI, PCoeffProcessingContextEighth& context, ValidatorWorkerData& workerData, int jobToFree) {
	const OutputBuffer& resultBuf = workerData.jobs[jobToFree];
	NodeIndex topIdx = resultBuf.originalInputData.getTop();
	size_t numBottoms = resultBuf.originalInputData.getNumberOfBottoms();
	context.inputBufferAlloc.push(resultBuf.originalInputData.bufStart);
	context.resultBufferAlloc.push(resultBuf.outputBuf);
	validatorFinishMessage(complexI, topIdx, numBottoms, workerData.processedCounts[jobToFree].load(), workerData.startTimes[jobToFree]);
}

void* noValidatorPThread(void* voidData) {
	ValidatorThreadData* validatorData = (ValidatorThreadData*) voidData;
	PCoeffProcessingContextEighth& context = *validatorData->context;
	setThreadName(("NoValidator " + std::to_string(validatorData->complexI)).c_str());
	validatorOnlineMessage(validatorData->complexI);

	for(std::optional<OutputBuffer> outputBuffer; (outputBuffer = context.validationQueue.pop_wait()).has_value(); ) {
		OutputBuffer outBuf = outputBuffer.value();

		context.inputBufferAlloc.push(outBuf.originalInputData.bufStart);
		context.resultBufferAlloc.push(outBuf.outputBuf);
	}

	validatorExitMessage(validatorData->complexI);
	pthread_exit(nullptr);
	return nullptr;
}
