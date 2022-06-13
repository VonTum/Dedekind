#include "commands.h"

#include "../dedelib/flatPCoeffProcessing.h"
#include "../dedelib/supercomputerJobs.h"

#include <sstream>

template<unsigned int Variables>
static void processSuperComputingJob_ST(const std::string& arg) {
	size_t separatorIdx = arg.find_last_of(",");
	std::string projectFolderPath = arg.substr(0, separatorIdx);
	int jobIndex = std::stoi(arg.substr(separatorIdx+1));
	processJob<Variables, 128>(projectFolderPath, jobIndex, "cpuST", cpuProcessor_SingleThread<Variables>);
}

template<unsigned int Variables>
static void processSuperComputingJob_FMT(const std::string& arg) {
	size_t separatorIdx = arg.find_last_of(",");
	std::string projectFolderPath = arg.substr(0, separatorIdx);
	int jobIndex = std::stoi(arg.substr(separatorIdx+1));
	processJob<Variables, 128>(projectFolderPath, jobIndex, "cpuFMT", cpuProcessor_FineMultiThread<Variables>);
}

std::vector<std::string> splitList(const std::string& strList) {
	std::stringstream strStream(strList);
	std::string segment;
	std::vector<std::string> results;

	while(std::getline(strStream, segment, ',')) {
		results.push_back(segment);
	}
	return results;
}

CommandSet processingCommands {"Massively parallel Processing Commands", {
	{"processDedekindNumber1_ST", []() {processDedekindNumber<1, 32>(cpuProcessor_SingleThread<1>); }},
	{"processDedekindNumber2_ST", []() {processDedekindNumber<2, 32>(cpuProcessor_SingleThread<2>); }},
	{"processDedekindNumber3_ST", []() {processDedekindNumber<3, 32>(cpuProcessor_SingleThread<3>); }},
	{"processDedekindNumber4_ST", []() {processDedekindNumber<4, 32>(cpuProcessor_SingleThread<4>); }},
	{"processDedekindNumber5_ST", []() {processDedekindNumber<5, 32>(cpuProcessor_SingleThread<5>); }},
	{"processDedekindNumber6_ST", []() {processDedekindNumber<6, 32>(cpuProcessor_SingleThread<6>); }},
	{"processDedekindNumber7_ST", []() {processDedekindNumber<7, 32>(cpuProcessor_SingleThread<7>); }},

	{"processDedekindNumber1_CMT", []() {processDedekindNumber<1, 32>(cpuProcessor_CoarseMultiThread<1>); }},
	{"processDedekindNumber2_CMT", []() {processDedekindNumber<2, 32>(cpuProcessor_CoarseMultiThread<2>); }},
	{"processDedekindNumber3_CMT", []() {processDedekindNumber<3, 32>(cpuProcessor_CoarseMultiThread<3>); }},
	{"processDedekindNumber4_CMT", []() {processDedekindNumber<4, 32>(cpuProcessor_CoarseMultiThread<4>); }},
	{"processDedekindNumber5_CMT", []() {processDedekindNumber<5, 32>(cpuProcessor_CoarseMultiThread<5>); }},
	{"processDedekindNumber6_CMT", []() {processDedekindNumber<6, 32>(cpuProcessor_CoarseMultiThread<6>); }},
	{"processDedekindNumber7_CMT", []() {processDedekindNumber<7, 32>(cpuProcessor_CoarseMultiThread<7>); }},

	{"processDedekindNumber1_FMT", []() {processDedekindNumber<1, 32>(cpuProcessor_FineMultiThread<1>); }},
	{"processDedekindNumber2_FMT", []() {processDedekindNumber<2, 32>(cpuProcessor_FineMultiThread<2>); }},
	{"processDedekindNumber3_FMT", []() {processDedekindNumber<3, 32>(cpuProcessor_FineMultiThread<3>); }},
	{"processDedekindNumber4_FMT", []() {processDedekindNumber<4, 32>(cpuProcessor_FineMultiThread<4>); }},
	{"processDedekindNumber5_FMT", []() {processDedekindNumber<5, 32>(cpuProcessor_FineMultiThread<5>); }},
	{"processDedekindNumber6_FMT", []() {processDedekindNumber<6, 32>(cpuProcessor_FineMultiThread<6>); }},
	{"processDedekindNumber7_FMT", []() {processDedekindNumber<7, 32>(cpuProcessor_FineMultiThread<7>); }},
}, {
	{"initializeSupercomputingProject", [](const std::string& project) {
		std::vector<std::string> args = splitList(project);
		std::string projectFolderPath = args[0];
		unsigned int targetDedekindNumber = std::stoi(args[1]);
		size_t numberOfJobs = std::stoi(args[2]);
		size_t topsPerBatch = args.size() >= 4 ? std::stoi(args[3]) : 128;
		initializeComputeProject(projectFolderPath, targetDedekindNumber, numberOfJobs, topsPerBatch);
	}},
	{"collectAllSupercomputingProjectResults_D3", collectAndProcessResults<1>},
	{"collectAllSupercomputingProjectResults_D4", collectAndProcessResults<2>},
	{"collectAllSupercomputingProjectResults_D5", collectAndProcessResults<3>},
	{"collectAllSupercomputingProjectResults_D6", collectAndProcessResults<4>},
	{"collectAllSupercomputingProjectResults_D7", collectAndProcessResults<5>},
	{"collectAllSupercomputingProjectResults_D8", collectAndProcessResults<6>},
	{"collectAllSupercomputingProjectResults_D9", collectAndProcessResults<7>},

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
}};
