#include "commands.h"

#include "../dedelib/flatPCoeffProcessing.h"
#include "../dedelib/supercomputerJobs.h"

#include <sstream>

template<unsigned int Variables>
static void processSuperComputingJob_ST(const std::string& arg) {
	size_t separatorIdx = arg.find_last_of(",");
	std::string projectFolderPath = arg.substr(0, separatorIdx);
	int jobIndex = std::stoi(arg.substr(separatorIdx+1));
	processJob<Variables>(projectFolderPath, jobIndex, "cpuST", cpuProcessor_SingleThread<Variables>);
}

template<unsigned int Variables>
static void processSuperComputingJob_FMT(const std::string& arg) {
	size_t separatorIdx = arg.find_last_of(",");
	std::string projectFolderPath = arg.substr(0, separatorIdx);
	int jobIndex = std::stoi(arg.substr(separatorIdx+1));
	processJob<Variables>(projectFolderPath, jobIndex, "cpuFMT", cpuProcessor_FineMultiThread<Variables>);
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

inline void benchmarkBottomBufferProduction(const std::string& arg) {
	size_t separatorIdxA = arg.find_first_of(",");
	size_t separatorIdxB = arg.find_last_of(",");
	unsigned int Variables = std::stoi(arg.substr(0, separatorIdxA));
	int sampleCount = std::stoi(arg.substr(separatorIdxA+1, separatorIdxB));
	int threadCount = std::stoi(arg.substr(separatorIdxB+1));

	const FlatNode* flatNodes = readFlatBuffer<FlatNode>(FileName::flatNodes(Variables), mbfCounts[Variables] + 1);
	auto tops = generateRangeSample(Variables, sampleCount);
	auto jobTops = convertTopInfos(flatNodes, tops);
	std::promise<std::vector<JobTopInfo>> jobTopsPromise;
	jobTopsPromise.set_value(jobTops);

	PCoeffProcessingContext context(Variables, 200, 0);

	std::cout << "Files loaded. Starting benchmark." << std::endl;

	std::thread loopBack([&](){
		auto startTime = std::chrono::high_resolution_clock::now();
		size_t bufferI = 0;
		while(true) {
			auto optBuf = context.inputQueue.pop_wait();
			if(optBuf.has_value()) {
				double secondsSinceStart = ((std::chrono::high_resolution_clock::now() - startTime).count() * 1.0e-9);
				std::cout << "Buffer " << bufferI++ << " received at " << secondsSinceStart << "s" << std::endl;
				context.inputBufferReturnQueue.push(optBuf.value().bufStart);
			} else {
				break;
			}
		}
	});
	auto jobTopsFuture = jobTopsPromise.get_future();
	runBottomBufferCreator(Variables, jobTopsFuture, context.inputQueue, context.inputBufferReturnQueue, threadCount);

	loopBack.join();
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
}, {
	{"initializeSupercomputingProject", [](const std::string& project) {
		std::vector<std::string> args = splitList(project);
		std::string projectFolderPath = args[0];
		unsigned int targetDedekindNumber = std::stoi(args[1]);
		size_t numberOfJobs = std::stoi(args[2]);
		size_t topsPerBatch = args.size() >= 4 ? std::stoi(args[3]) : 128;
		initializeComputeProject(projectFolderPath, targetDedekindNumber, numberOfJobs, topsPerBatch);
	}},

	{"benchmarkBottomBufferProduction", [](const std::string& arg) {benchmarkBottomBufferProduction(arg);}},

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
