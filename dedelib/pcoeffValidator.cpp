#include "pcoeffValidator.h"

#include <iostream>
#include <string>


void validatorStartMessage(NodeIndex topIdx, size_t numBottoms) {
	std::cout << "\033[35m[Validator] Validating top " + std::to_string(topIdx) + " with " + std::to_string(numBottoms) + " bottoms\033[39m\n" << std::flush;
}
void validatorFinishMessage(NodeIndex topIdx, size_t numBottoms, size_t numTestedPCoeffs, std::chrono::time_point<std::chrono::high_resolution_clock> startTime) {
	std::chrono::nanoseconds deltaTime = std::chrono::high_resolution_clock::now() - startTime;
	std::cout << "\033[35m[Validator] Correct top " + std::to_string(topIdx) + " with " + std::to_string(numBottoms) + " bottoms: " + std::to_string(numTestedPCoeffs) + " p-coefficients checked in " + std::to_string(deltaTime.count() / 1000000.0) + "ms\033[39m\n" << std::flush;
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

void NO_VALIDATOR(const OutputBuffer& resultBuf, const void* voidMBFs, ThreadPool& pool) {}
