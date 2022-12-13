#include "commands.h"

#include "../dedelib/supercomputerJobs.h"
#include "../dedelib/pcoeffValidator.h"
#include "../dedelib/singleTopVerification.h"

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
	bool success = processJob(Variables, projectFolderPath, jobID, name, processorFunc, validator);

	if(!success) std::abort();
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

template<unsigned int Variables>
void checkErrorBuffer(const std::vector<std::string>& args) {
	const std::string& fileName = args[0];

	NodeIndex* idxBuf = aligned_mallocT<NodeIndex>(mbfCounts[Variables], 4096);
	ProcessedPCoeffSum* resultBuf = aligned_mallocT<ProcessedPCoeffSum>(mbfCounts[Variables], 4096);
	ProcessedPCoeffSum* correctResultBuf = aligned_mallocT<ProcessedPCoeffSum>(mbfCounts[Variables], 4096);
	uint64_t bufSize = readProcessingBufferPairFromFile(fileName.c_str(), idxBuf, resultBuf);

	const Monotonic<Variables>* mbfs = readFlatBufferNoMMAP<Monotonic<Variables>>(FileName::flatMBFs(Variables), mbfCounts[Variables]);
	ThreadPool threadPool;
	JobInfo job;
	job.bufStart = idxBuf;
	job.bufEnd = idxBuf + bufSize;
	// Don't need job.blockEnd
	processBetasCPU_MultiThread(mbfs, job, correctResultBuf, threadPool);

	size_t firstErrorIdx;
	for(size_t i = 2; i < bufSize; i++) {
		if(resultBuf[i] != correctResultBuf[i]) {
			firstErrorIdx = i;
			goto errorFound;
		}
	}
	std::cout << std::to_string(bufSize) + " elements checked. All correct!" << std::flush;
	return;
	errorFound:
	size_t lastErrorIdx = firstErrorIdx;
	size_t numberOfErrors = 1;
	for(size_t i = firstErrorIdx+1; i < bufSize; i++) {
		if(resultBuf[i] != correctResultBuf[i]) {
			lastErrorIdx = i;
			numberOfErrors++;
		}
	}

	for(size_t i = firstErrorIdx-5; i <= lastErrorIdx+5; i++) {
		ProcessedPCoeffSum found = resultBuf[i];
		ProcessedPCoeffSum correct = correctResultBuf[i];
		NodeIndex idx = idxBuf[i];
		const char* alignColor = (i % 32 >= 16) ? "\n\033[37m" : "\n\033[39m";
		std::cout << alignColor + std::to_string(idx) + "> \033[39m";

		if(found != correct) {
			std::cout << 
				"sum: \033[31m" + std::to_string(getPCoeffSum(found)) + "\033[39m / \033[32m" + std::to_string(getPCoeffSum(correct))
				+ "\033[39m \tcount: \033[31m" + std::to_string(getPCoeffCount(found)) + "\033[39m / \033[32m" + std::to_string(getPCoeffCount(correct)) + "\033[39m";
		} else {
			std::cout << "\033[32mCorrect!\033[39m";
		}
	}

	std::cout << std::flush;

	size_t errorRangeSize = lastErrorIdx - firstErrorIdx + 1;
	std::cout << "\nFirst Error: " + std::to_string(firstErrorIdx) + " (bot " + std::to_string(idxBuf[firstErrorIdx]) + ")";
	std::cout << "\nLast Error: " + std::to_string(lastErrorIdx) + " (bot " + std::to_string(idxBuf[lastErrorIdx]) + ")";
	std::cout << "\nNumber of Errors: " + std::to_string(numberOfErrors) + " / " + std::to_string(bufSize);
	std::cout << "\nError index range size: " + std::to_string(errorRangeSize) + " (" + std::to_string(numberOfErrors * 100.0 / errorRangeSize) + ")";
	std::cout << "\n" << std::flush;
}

template<typename IntType>
std::string toHex(IntType v) {
	constexpr char chars[16]{'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
	char outStr[sizeof(IntType)*2];
	for(size_t i = 0; i < sizeof(IntType) * 2; i++) {
		IntType quartet = (v >> (sizeof(IntType) * 8 - 4 * i - 4)) & 0b00001111;
		outStr[i] = chars[quartet];
	}
	return std::string(outStr, sizeof(IntType) * 2);
}

void printErrorBuffer(const std::vector<std::string>& args) {
	std::string fileName = args[0];

	NodeIndex* idxBuf = aligned_mallocT<NodeIndex>(mbfCounts[7], 4096);
	ProcessedPCoeffSum* resultsBuf = aligned_mallocT<ProcessedPCoeffSum>(mbfCounts[7], 4096);
	uint64_t bufSize = readProcessingBufferPairFromFile(fileName.c_str(), idxBuf, resultsBuf);

	uint64_t from = 0;
	uint64_t to;
	if(args.size() > 1) {
		from = std::stoll(args[1]);
		to = std::stoll(args[2]);
		if(bufSize < to) to = bufSize;
	} else {
		to = bufSize;
	}

	std::cout << "Buffer for top " + std::to_string(idxBuf[0] & 0x7FFFFFFF) + " of size " + std::to_string(bufSize) + "\n";

	std::cout << "Start at idx " + std::to_string(from) << ":\n";
	for(uint64_t i = from; i < to; i++) {
		ProcessedPCoeffSum ps = resultsBuf[i];
		const char* selectedColor = (i % 32 >= 16) ? "\033[37m" : "\033[39m";
		if(i % 512 == 0) selectedColor = "\033[34m";
		std::string hexText = toHex(ps);
		if(hexText[0] == 'e') {
			hexText = "\033[31m" + hexText + selectedColor;
		}
		if(hexText[0] == 'f') {
			hexText = "\033[32m" + hexText + selectedColor;
		}
		std::cout << selectedColor + std::to_string(idxBuf[i]) + "> " + hexText + "  sum: " + std::to_string(getPCoeffSum(ps)) + " count: " + std::to_string(getPCoeffCount(ps)) + "\033[39m\n";
	}
	std::cout << "Up to idx " + std::to_string(to) << "\n";
}

template<unsigned int Variables>
void checkSingleTopResult(BetaResult originalResult) {
	std::cout << "Checking top " + std::to_string(originalResult.topIndex) + "..." << std::endl;

	std::cout << "Original result: " + toString(originalResult) << std::endl;
	SingleTopResult realSol = computeSingleTopWithAllCores<Variables, true>(originalResult.topIndex);

	std::cout << "Original result: " + toString(originalResult) << std::endl;
	std::cout << "Correct result:  result: " + toString(realSol.resultSum) + ", dual: " + toString(realSol.dualSum) << std::endl;

	if(originalResult.dataForThisTop.betaSum == realSol.resultSum && originalResult.dataForThisTop.betaSumDualDedup == realSol.dualSum) {
		std::cout << "\033[32mCorrect!\033[39m\n" << std::flush;
	} else {
		std::cout << "\033[31mBad Result!\033[39m\n" << std::flush;
		std::abort();
	}
}

template<unsigned int Variables>
void checkResultsFileSamples(const std::vector<std::string>& args) {
	const std::string& filePath = args[0];

	ValidationData unused_checkSum;
	std::vector<BetaResult> results = readResultsFile(Variables, filePath.c_str(), unused_checkSum);

	if(args.size() > 1) {
		for(size_t i = 1; i < args.size(); i++) {
			size_t elemIndex = std::stoi(args[i]);
			std::cout << "Checking result idx " + std::to_string(elemIndex) + " / " + std::to_string(results.size()) << std::endl;
			if(elemIndex >= results.size()) {
				std::cerr << "ERROR Result index out of bounds! " + std::to_string(elemIndex) + " / " + std::to_string(results.size()) << std::endl;
				std::abort();
			}

			checkSingleTopResult<Variables>(results[elemIndex]);
		}
	} else {
		std::default_random_engine generator;
		std::uniform_int_distribution<size_t> distr(0, results.size() - 1);
		size_t selectedResult = distr(generator);
		std::cout << "Checking result idx " + std::to_string(selectedResult) + " / " + std::to_string(results.size()) << std::endl;
		checkSingleTopResult<Variables>(results[selectedResult]);
	}
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

	{"resetUnfinishedJobs", [](const std::vector<std::string>& args){
		size_t lowerBound = 0;
		size_t upperBound = 99999999;
		if(args.size() >= 3) {
			lowerBound = std::stoi(args[2]);
			upperBound = std::stoi(args[3]);
		}

		resetUnfinishedJobs(args[0], args[1], lowerBound, upperBound);
	}},
	{"checkProjectResultsIdentical", [](const std::vector<std::string>& args){checkProjectResultsIdentical(std::stoi(args[0]) - 2, args[1], args[2]);}},

	{"collectAllSupercomputingProjectResults", [](const std::vector<std::string>& args){collectAndProcessResults(std::stoi(args[0]) - 2, args[1]);}},

	{"checkProjectIntegrity", [](const std::vector<std::string>& args){checkProjectIntegrity(std::stoi(args[0]) - 2, args[1]);}},

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

	{"checkErrorBuffer1", checkErrorBuffer<1>},
	{"checkErrorBuffer2", checkErrorBuffer<2>},
	{"checkErrorBuffer3", checkErrorBuffer<3>},
	{"checkErrorBuffer4", checkErrorBuffer<4>},
	{"checkErrorBuffer5", checkErrorBuffer<5>},
	{"checkErrorBuffer6", checkErrorBuffer<6>},
	{"checkErrorBuffer7", checkErrorBuffer<7>},

	{"printErrorBuffer", printErrorBuffer},

	{"checkResultsFileSamples1", checkResultsFileSamples<1>},
	{"checkResultsFileSamples2", checkResultsFileSamples<2>},
	{"checkResultsFileSamples3", checkResultsFileSamples<3>},
	{"checkResultsFileSamples4", checkResultsFileSamples<4>},
	{"checkResultsFileSamples5", checkResultsFileSamples<5>},
	{"checkResultsFileSamples6", checkResultsFileSamples<6>},
	{"checkResultsFileSamples7", checkResultsFileSamples<7>},
}};
