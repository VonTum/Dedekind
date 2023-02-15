#include "commands.h"

#include <iostream>

#include "../dedelib/singleTopVerification.h"
#include "../dedelib/supercomputerJobs.h"
#include "../dedelib/flatPCoeffProcessing.h"

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

template<unsigned int Variables>
void checkValidationFileCPU(const std::vector<std::string>& args) {
	const std::string& validationFileName = args[0];
	int numSamples = mbfCounts[Variables] / 20;
	if(args.size() >= 2) {
		numSamples = std::stoi(args[1]);
	}
	
	
	// TODO validationBufferValidationPipeline(Variables, vData, cpuProcessor_SuperMultiThread<Variables>)
	
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

	{"checkValidationFileCPU_1", checkValidationFileCPU<1>},
	{"checkValidationFileCPU_2", checkValidationFileCPU<2>},
	{"checkValidationFileCPU_3", checkValidationFileCPU<3>},
	{"checkValidationFileCPU_4", checkValidationFileCPU<4>},
	{"checkValidationFileCPU_5", checkValidationFileCPU<5>},
	{"checkValidationFileCPU_6", checkValidationFileCPU<6>},
	{"checkValidationFileCPU_7", checkValidationFileCPU<7>},
}};
