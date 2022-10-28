#include "commands.h"

#include "../dedelib/supercomputerJobs.h"
#include "../dedelib/pcoeffValidator.h"

#include <sstream>

template<unsigned int Variables>
static void processSuperComputingJob_FromArgs(const std::vector<std::string>& args, const char* name, void (*processorFunc)(PCoeffProcessingContext&)) {
	std::string projectFolderPath = args[0];
	std::string jobID = args[1];
	void*(*validator)(void*) = nullptr;
	if(args.size() >= 3) {
		if(args[2] == "validate_basic") {
			validator = basicValidatorPThread<Variables>;
		} else if(args[2] == "validate_adv") {
			validator = continuousValidatorPThread<Variables>;
		}
	}
	processJob(Variables, projectFolderPath, jobID, name, processorFunc, validator);
}

template<unsigned int Variables>
static void processSuperComputingJob_ST(const std::vector<std::string>& args) {
	processSuperComputingJob_FromArgs<Variables>(args, "cpuST", cpuProcessor_SingleThread<Variables>);
}

template<unsigned int Variables>
static void processSuperComputingJob_FMT(const std::vector<std::string>& args) {
	processSuperComputingJob_FromArgs<Variables>(args, "cpuFMT", cpuProcessor_FineMultiThread<Variables>);
}

template<unsigned int Variables>
static void processSuperComputingJob_SMT(const std::vector<std::string>& args) {
	processSuperComputingJob_FromArgs<Variables>(args, "cpuSMT", cpuProcessor_SuperMultiThread<Variables>);
}

CommandSet superCommands {"Supercomputing Commands", {}, {

	{"initializeSupercomputingProject", [](const std::vector<std::string>& args) {
		std::string projectFolderPath = args[0];
		unsigned int targetDedekindNumber = std::stoi(args[1]);
		size_t numberOfJobs = std::stoi(args[2]);
		size_t numberOfJobsToActuallyGenerate = numberOfJobs;
		if(args.size() >= 4) {
			numberOfJobsToActuallyGenerate = std::stoi(args[3]);
			std::cerr << "WARNING: GENERATING ONLY PARTIAL JOBS:" << numberOfJobsToActuallyGenerate << '/' << numberOfJobs << " -> INVALID COMPUTE PROJECT!\n" << std::flush;
		}
		initializeComputeProject(targetDedekindNumber - 2, projectFolderPath, numberOfJobs, numberOfJobsToActuallyGenerate);
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

	{"resetUnfinishedJobs", [](const std::vector<std::string>& args){resetUnfinishedJobs(args[0]);}},
	{"checkProjectResultsIdentical", [](const std::vector<std::string>& args){checkProjectResultsIdentical(std::stoi(args[0]) - 2, args[1], args[2]);}},

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
