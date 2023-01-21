#include "pcoeffClasses.h"

#include "knownData.h"
#include <iostream>

BetaResultCollector::BetaResultCollector(unsigned int Variables) :
	allBetaSums(mbfCounts[Variables]),
	hasSeenResult(mbfCounts[Variables], false) {}

void BetaResultCollector::addBetaResult(BetaResult result) {
	if(hasSeenResult[result.topIndex]) {
		std::cerr << "Error: Duplicate beta result for topIdx " << result.topIndex << "! Aborting!" << std::endl;
		//std::abort();
	} else {
		hasSeenResult[result.topIndex] = true;
		allBetaSums[result.topIndex] = result.dataForThisTop;
	}
}
void BetaResultCollector::addBetaResults(const std::vector<BetaResult>& results) {
	for(BetaResult r : results) {
		this->addBetaResult(r);
	}
}

void BetaResultCollector::removeBetaResult(int resultIndex) {
	if(!hasSeenResult[resultIndex]) {
		std::cerr << "Error: Trying to remove un-added betaResult " << resultIndex << "! Aborting!" << std::endl;
		std::abort();
	} else {
		hasSeenResult[resultIndex] = false;
	}
}

bool BetaResultCollector::hasAllResults() const {
	for(size_t i = 0; i < hasSeenResult.size(); i++) {
		if(!hasSeenResult[i]) {
			return false;
		}
	}
	return true;
}

std::vector<BetaSumPair> BetaResultCollector::getResultingSums() {
	for(size_t i = 0; i < hasSeenResult.size(); i++) {
		if(!hasSeenResult[i]) {
			std::cerr << "No Result for top index " << i << " computation not complete! Aborting!";
			std::abort();
		}
	}
	return allBetaSums;
}

static BetaSum safeDiv(BetaSum sum, uint32_t divisor) {
	BetaSum totalModSum = sum % divisor;
	if(totalModSum.countedIntervalSizeDown != 0) {
		std::cerr << "BetaSum count not safely divisible by " << divisor << "! Aborting!";
		std::abort();
	}
	if(totalModSum.betaSum != 0) {
		std::cerr << "BetaSum sum not safely divisible by " << divisor << "! Aborting!";
		std::abort();
	}
	return sum / divisor;
}
BetaSum BetaSumPair::getBetaSum(unsigned int Variables) const {
	BetaSum totalSum = betaSum + betaSum + betaSumDualDedup;
	return safeDiv(totalSum, factorial(Variables));
}
BetaSum BetaSumPair::getBetaSumPlusValidationTerm(unsigned int Variables, BetaSum validationTerm) const {
	BetaSum totalSum = betaSum + validationTerm + betaSumDualDedup;
	return safeDiv(totalSum, factorial(Variables));
}

std::string toString(const BetaSum& betaSum) {
	return "bs:" + toString(betaSum.betaSum) + ", isd:" + std::to_string(betaSum.countedIntervalSizeDown);
}

std::string toString(const BetaResult& br) {
	return "top " + std::to_string(br.topIndex) + " =>  betaSum: " + toString(br.dataForThisTop.betaSum) + ";  dualDedup: " + toString(br.dataForThisTop.betaSumDualDedup);
}
