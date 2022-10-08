#include "commands.h"

#include "../dedelib/flatPCoeffProcessing.h"
#include "../dedelib/supercomputerJobs.h"

#include "../dedelib/pcoeffValidator.h"
#include "../dedelib/threadPool.h"
#include "../dedelib/pcoeffClasses.h"

#include <sstream>

template<unsigned int Variables>
static void processSuperComputingJob_ST(const std::vector<std::string>& args) {
	std::string projectFolderPath = args[0];
	std::string jobID = args[1];
	bool validate = args.size() >= 3;
	processJob(Variables, projectFolderPath, jobID, "cpuST", cpuProcessor_SingleThread<Variables>, validate ? threadPoolBufferValidator<Variables> : nullptr);
}

template<unsigned int Variables>
static void processSuperComputingJob_FMT(const std::vector<std::string>& args) {
	std::string projectFolderPath = args[0];
	std::string jobID = args[1];
	bool validate = args.size() >= 3;
	processJob(Variables, projectFolderPath, jobID, "cpuFMT", cpuProcessor_FineMultiThread<Variables>, validate ? threadPoolBufferValidator<Variables> : nullptr);
}

template<unsigned int Variables>
static void processSuperComputingJob_SMT(const std::vector<std::string>& args) {
	std::string projectFolderPath = args[0];
	std::string jobID = args[1];
	bool validate = args.size() >= 3;
	processJob(Variables, projectFolderPath, jobID, "cpuSMT", cpuProcessor_SuperMultiThread<Variables>, validate ? threadPoolBufferValidator<Variables> : nullptr);
}

template<unsigned int Variables>
void processDedekindNumberWithValidator(void (*processorFunc)(PCoeffProcessingContext& context, const void* mbfs[2])) {
	processDedekindNumber<Variables>(processorFunc, threadPoolBufferValidator<Variables>);
}

CommandSet processingCommands {"Massively parallel Processing Commands", {
	{"processDedekindNumber1_ST", []() {processDedekindNumber<1>(cpuProcessor_SingleThread<1>); }},
	{"processDedekindNumber2_ST", []() {processDedekindNumber<2>(cpuProcessor_SingleThread<2>); }},
	{"processDedekindNumber3_ST", []() {processDedekindNumber<3>(cpuProcessor_SingleThread<3>); }},
	{"processDedekindNumber4_ST", []() {processDedekindNumber<4>(cpuProcessor_SingleThread<4>); }},
	{"processDedekindNumber5_ST", []() {processDedekindNumber<5>(cpuProcessor_SingleThread<5>); }},
	{"processDedekindNumber6_ST", []() {processDedekindNumber<6>(cpuProcessor_SingleThread<6>); }},
	{"processDedekindNumber7_ST", []() {processDedekindNumber<7>(cpuProcessor_SingleThread<7>); }},

	{"processDedekindNumber1_CMT", []() {processDedekindNumber<1>(cpuProcessor_CoarseMultiThread<1>); }},
	{"processDedekindNumber2_CMT", []() {processDedekindNumber<2>(cpuProcessor_CoarseMultiThread<2>); }},
	{"processDedekindNumber3_CMT", []() {processDedekindNumber<3>(cpuProcessor_CoarseMultiThread<3>); }},
	{"processDedekindNumber4_CMT", []() {processDedekindNumber<4>(cpuProcessor_CoarseMultiThread<4>); }},
	{"processDedekindNumber5_CMT", []() {processDedekindNumber<5>(cpuProcessor_CoarseMultiThread<5>); }},
	{"processDedekindNumber6_CMT", []() {processDedekindNumber<6>(cpuProcessor_CoarseMultiThread<6>); }},
	{"processDedekindNumber7_CMT", []() {processDedekindNumber<7>(cpuProcessor_CoarseMultiThread<7>); }},

	{"processDedekindNumber1_FMT", []() {processDedekindNumber<1>(cpuProcessor_FineMultiThread<1>); }},
	{"processDedekindNumber2_FMT", []() {processDedekindNumber<2>(cpuProcessor_FineMultiThread<2>); }},
	{"processDedekindNumber3_FMT", []() {processDedekindNumber<3>(cpuProcessor_FineMultiThread<3>); }},
	{"processDedekindNumber4_FMT", []() {processDedekindNumber<4>(cpuProcessor_FineMultiThread<4>); }},
	{"processDedekindNumber5_FMT", []() {processDedekindNumber<5>(cpuProcessor_FineMultiThread<5>); }},
	{"processDedekindNumber6_FMT", []() {processDedekindNumber<6>(cpuProcessor_FineMultiThread<6>); }},
	{"processDedekindNumber7_FMT", []() {processDedekindNumber<7>(cpuProcessor_FineMultiThread<7>); }},

	{"processDedekindNumber1_SMT", []() {processDedekindNumber<1>(cpuProcessor_SuperMultiThread<1>); }},
	{"processDedekindNumber2_SMT", []() {processDedekindNumber<2>(cpuProcessor_SuperMultiThread<2>); }},
	{"processDedekindNumber3_SMT", []() {processDedekindNumber<3>(cpuProcessor_SuperMultiThread<3>); }},
	{"processDedekindNumber4_SMT", []() {processDedekindNumber<4>(cpuProcessor_SuperMultiThread<4>); }},
	{"processDedekindNumber5_SMT", []() {processDedekindNumber<5>(cpuProcessor_SuperMultiThread<5>); }},
	{"processDedekindNumber6_SMT", []() {processDedekindNumber<6>(cpuProcessor_SuperMultiThread<6>); }},
	{"processDedekindNumber7_SMT", []() {processDedekindNumber<7>(cpuProcessor_SuperMultiThread<7>); }},

	{"processDedekindNumber1_FMT_Validated", []() {processDedekindNumberWithValidator<1>(cpuProcessor_FineMultiThread<1>); }},
	{"processDedekindNumber2_FMT_Validated", []() {processDedekindNumberWithValidator<2>(cpuProcessor_FineMultiThread<2>); }},
	{"processDedekindNumber3_FMT_Validated", []() {processDedekindNumberWithValidator<3>(cpuProcessor_FineMultiThread<3>); }},
	{"processDedekindNumber4_FMT_Validated", []() {processDedekindNumberWithValidator<4>(cpuProcessor_FineMultiThread<4>); }},
	{"processDedekindNumber5_FMT_Validated", []() {processDedekindNumberWithValidator<5>(cpuProcessor_FineMultiThread<5>); }},
	{"processDedekindNumber6_FMT_Validated", []() {processDedekindNumberWithValidator<6>(cpuProcessor_FineMultiThread<6>); }},
	{"processDedekindNumber7_FMT_Validated", []() {processDedekindNumberWithValidator<7>(cpuProcessor_FineMultiThread<7>); }},
}, {
	{"initializeSupercomputingProject", [](const std::vector<std::string>& args) {
		std::string projectFolderPath = args[0];
		unsigned int targetDedekindNumber = std::stoi(args[1]);
		size_t numberOfJobs = std::stoi(args[2]);
		size_t topsPerBatch = args.size() >= 4 ? std::stoi(args[3]) : 8;
		initializeComputeProject(targetDedekindNumber - 2, projectFolderPath, numberOfJobs, topsPerBatch);
	}},

	{"initializeValidationFiles", [](const std::vector<std::string>& args) {
		std::string projectFolderPath = args[0];
		unsigned int targetDedekindNumber = std::stoi(args[1]);
		std::vector<std::string> validationComputeIDs;
		for(size_t i = 2; i < args.size(); i++) {
			validationComputeIDs.push_back(args[i]);
		}
		initializeValidationFiles(targetDedekindNumber - 2, projectFolderPath, validationComputeIDs);
	}},

	{"collectAllSupercomputingProjectResults_D3", [](const std::vector<std::string>& args){collectAndProcessResults<1>(args[0]);}},
	{"collectAllSupercomputingProjectResults_D4", [](const std::vector<std::string>& args){collectAndProcessResults<2>(args[0]);}},
	{"collectAllSupercomputingProjectResults_D5", [](const std::vector<std::string>& args){collectAndProcessResults<3>(args[0]);}},
	{"collectAllSupercomputingProjectResults_D6", [](const std::vector<std::string>& args){collectAndProcessResults<4>(args[0]);}},
	{"collectAllSupercomputingProjectResults_D7", [](const std::vector<std::string>& args){collectAndProcessResults<5>(args[0]);}},
	{"collectAllSupercomputingProjectResults_D8", [](const std::vector<std::string>& args){collectAndProcessResults<6>(args[0]);}},
	{"collectAllSupercomputingProjectResults_D9", [](const std::vector<std::string>& args){collectAndProcessResults<7>(args[0]);}},

	{"processJobCPU1_ST", processSuperComputingJob_ST<1>},
	{"processJobCPU2_ST", processSuperComputingJob_ST<2>},
	{"processJobCPU3_ST", processSuperComputingJob_ST<3>},
	{"processJobCPU4_ST", processSuperComputingJob_ST<4>},
	{"processJobCPU5_ST", processSuperComputingJob_ST<5>},
	{"processJobCPU6_ST", processSuperComputingJob_ST<6>},
	{"processJobCPU7_ST", processSuperComputingJob_ST<7>},

	{"processJobCPU1_FMT", processSuperComputingJob_FMT<1>},
	{"processJobCPU2_FMT", processSuperComputingJob_FMT<2>},
	{"processJobCPU3_FMT", processSuperComputingJob_FMT<3>},
	{"processJobCPU4_FMT", processSuperComputingJob_FMT<4>},
	{"processJobCPU5_FMT", processSuperComputingJob_FMT<5>},
	{"processJobCPU6_FMT", processSuperComputingJob_FMT<6>},
	{"processJobCPU7_FMT", processSuperComputingJob_FMT<7>},

	{"processJobCPU1_SMT", processSuperComputingJob_SMT<1>},
	{"processJobCPU2_SMT", processSuperComputingJob_SMT<2>},
	{"processJobCPU3_SMT", processSuperComputingJob_SMT<3>},
	{"processJobCPU4_SMT", processSuperComputingJob_SMT<4>},
	{"processJobCPU5_SMT", processSuperComputingJob_SMT<5>},
	{"processJobCPU6_SMT", processSuperComputingJob_SMT<6>},
	{"processJobCPU7_SMT", processSuperComputingJob_SMT<7>},
}};
