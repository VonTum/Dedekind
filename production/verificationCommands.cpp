#include "commands.h"

#include <iostream>

#include "../dedelib/singleTopVerification.h"

template<unsigned int Variables, bool SkipValidation>
void getTopBetaSums(const std::vector<std::string>& args) {
	NodeIndex id = std::stoi(args[0]);
	SingleTopResult result = computeSingleTopWithAllCores<Variables, SkipValidation>(id);

	std::cout << "Results for top idx: " + std::to_string(id) << std::endl;
	std::cout << "result BetaSum: " + toString(result.resultSum) << std::endl;
	std::cout << "result DualSum: " + toString(result.dualSum) << std::endl;
	if(!SkipValidation) {
		std::cout << "result Validation: " + toString(result.validationSum) << std::endl;
		std::cout << "TOTAL: " + toString(result.resultSum + result.dualSum + result.validationSum) << std::endl;
	} else {
		std::cout << "result Validation: /" << std::endl;
		std::cout << "TOTAL: /" << std::endl;
	}
}

CommandSet verificationCommands{"Verification of results Commands", {}, {
	{"checkTopResult1", getTopBetaSums<1, true>},
	{"checkTopResult2", getTopBetaSums<2, true>},
	{"checkTopResult3", getTopBetaSums<3, true>},
	{"checkTopResult4", getTopBetaSums<4, true>},
	{"checkTopResult5", getTopBetaSums<5, true>},
	{"checkTopResult6", getTopBetaSums<6, true>},
	{"checkTopResult7", getTopBetaSums<7, true>},

	{"checkFullTopResult1", getTopBetaSums<1, false>},
	{"checkFullTopResult2", getTopBetaSums<2, false>},
	{"checkFullTopResult3", getTopBetaSums<3, false>},
	{"checkFullTopResult4", getTopBetaSums<4, false>},
	{"checkFullTopResult5", getTopBetaSums<5, false>},
	{"checkFullTopResult6", getTopBetaSums<6, false>},
	{"checkFullTopResult7", getTopBetaSums<7, false>},
}};
