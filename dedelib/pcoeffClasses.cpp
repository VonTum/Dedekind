#include "pcoeffClasses.h"

#include "knownData.h"
#include <iostream>

BetaResultCollector::BetaResultCollector(unsigned int Variables) :
	allBetaSums(mbfCounts[Variables]),
	hasSeenResult(mbfCounts[Variables], false) {}

void BetaResultCollector::addBetaResult(BetaResult result) {
	if(hasSeenResult[result.topIndex]) {
		std::cerr << "Error: Duplicate beta result for topIdx " << result.topIndex << "! Aborting!" << std::endl;
		std::abort();
	} else {
		hasSeenResult[result.topIndex] = true;
		allBetaSums[result.topIndex] = result.betaSum;
	}
}
void BetaResultCollector::addBetaResults(const std::vector<BetaResult>& results) {
	for(BetaResult r : results) {
		this->addBetaResult(r);
	}
}

std::vector<BetaSum> BetaResultCollector::getResultingSums() {
	for(size_t i = 0; i < hasSeenResult.size(); i++) {
		if(!hasSeenResult[i]) {
			std::cerr << "No Result for top index " << i << " computation not complete! Aborting!";
			std::abort();
		}
	}
	return allBetaSums;
}
