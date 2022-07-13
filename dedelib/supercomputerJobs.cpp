#include "supercomputerJobs.h"

#include <filesystem>
#include <random>
#include <algorithm>

#include <unistd.h>
#include <limits.h>

void checkFileDone(std::ifstream& file) {
	if(file.peek() != EOF) {
		std::cerr << "End of file not reached. Invalid file!" << std::endl;
		std::abort();
	}
}

std::string getComputeIdentifier() {
	char hostNameData[HOST_NAME_MAX+1];
	gethostname(hostNameData, HOST_NAME_MAX+1);
	return std::string(hostNameData);
}

static std::string computeFileName(unsigned int jobIndex, const std::string& extention) {
	std::string result = "j" + std::to_string(jobIndex) + extention;
	return result;
}

static std::string computeFolderPath(std::string computeFolder, const char* folder) {
	if(computeFolder[computeFolder.size() - 1] != '/') computeFolder.append("/");
	computeFolder.append(folder);
	computeFolder.append("/");
	return computeFolder;
}

static std::string computeFilePath(std::string computeFolder, const char* folder, unsigned int jobIndex, const std::string& extention) {
	return computeFolderPath(computeFolder, folder) + computeFileName(jobIndex, extention);
}

static void writeJobToFile(const std::string& jobFileName, unsigned int dedekindNumber, const std::vector<std::uint32_t>& topVector) {
	std::ofstream jobFile(jobFileName, std::ios::binary);

	serializeU32(dedekindNumber, jobFile);
	serializePODVector(topVector, jobFile);
}

void initializeComputeProject(std::string computeFolder, unsigned int dedekindNumber, size_t numberOfJobs, size_t topsPerBatch) {
	std::cout << "Generating a supercomputing project for computing Dedekind Number D(" << dedekindNumber << "). Split across " << numberOfJobs << " jobs. \nPath: " << computeFolder << std::endl;
	
	if(std::filesystem::exists(computeFolder)) {
		std::cerr << "The compute path already exists! (" << computeFolder << ") Choose a different compute folder!\nTermintating to avoid data loss. " << std::endl;
		std::abort();
	}

	std::cout << "Generating directories: " << computeFolder << "/\n    jobs/\n    working/\n    finished/\n    results/\n    logs/" << std::endl;
	std::filesystem::create_directory(computeFolder);
	std::filesystem::create_directory(computeFolderPath(computeFolder, "jobs"));
	std::filesystem::create_directory(computeFolderPath(computeFolder, "working"));
	std::filesystem::create_directory(computeFolderPath(computeFolder, "finished"));
	std::filesystem::create_directory(computeFolderPath(computeFolder, "results"));
	std::filesystem::create_directory(computeFolderPath(computeFolder, "logs"));

	size_t numberOfMBFsToProcess = mbfCounts[dedekindNumber-2];
	size_t numberOfCompleteBatches = numberOfMBFsToProcess / topsPerBatch;

	std::cout << "Collecting the " << numberOfMBFsToProcess << " MBFs for the full computation..." << std::endl;

	std::vector<size_t> completeBatchIndices;
	for(size_t i = 0; i < numberOfCompleteBatches; i++) {
		completeBatchIndices.push_back(i);
	}

	std::default_random_engine generator;
	std::shuffle(completeBatchIndices.begin(), completeBatchIndices.end(), generator);

	std::cout << "Generating " << numberOfJobs << " job files of ~" << (numberOfMBFsToProcess / numberOfJobs) << " MBFs per job..." << std::endl;

	size_t currentBatchIndex = 0;
	for(size_t jobI = 0; jobI < numberOfJobs; jobI++) {
		size_t jobsLeft = numberOfJobs - jobI;
		size_t topBatchesLeft = numberOfCompleteBatches - currentBatchIndex;
		size_t numberOfBatchesInThisJob = topBatchesLeft / jobsLeft;

		std::vector<std::uint32_t> topIndices;
		topIndices.reserve(numberOfBatchesInThisJob * topsPerBatch);
		for(size_t i = 0; i < numberOfBatchesInThisJob; i++) {
			std::uint32_t curBatchStart = completeBatchIndices[currentBatchIndex++] * topsPerBatch;
			for(size_t j = 0; j < topsPerBatch; j++) {
				topIndices.push_back(static_cast<std::uint32_t>(curBatchStart + j));
			}
		}
		// Add remaining tops to job 0
		if(jobI == 0) {
			size_t firstUnbatchedTop = numberOfCompleteBatches * topsPerBatch;
			for(size_t topI = firstUnbatchedTop; topI < numberOfMBFsToProcess; topI++) {
				topIndices.push_back(static_cast<std::uint32_t>(topI));
			}
		}

		writeJobToFile(computeFilePath(computeFolder, "jobs", jobI, ".job"), dedekindNumber, topIndices);
	}

	std::cout << "Generated " << numberOfJobs << " job files of ~" << (numberOfMBFsToProcess / numberOfJobs) << " MBFs per job." << std::endl;
}

std::vector<std::uint32_t> loadJob(unsigned int dedekindNumberProject, const std::string& computeFolder, const std::string& computeID, int jobIndex) {
	std::string jobFile = computeFilePath(computeFolder, "jobs", jobIndex, ".job");
	std::string workingFile = computeFilePath(computeFolder, "working", jobIndex, "_" + computeID + ".job");
	std::string resultsFile = computeFilePath(computeFolder, "results", jobIndex, "_" + computeID + ".results");
	
	if(std::filesystem::exists(workingFile)) {
		std::cerr << "Temporary file (" << workingFile << ") already exists! Job " << jobIndex << " is already running!";
		std::abort();
	}
	if(std::filesystem::exists(resultsFile)) {
		std::cerr << "Results file (" << resultsFile << ") already exists! Job " << jobIndex << " has already terminated!";
		std::abort();
	}
	std::filesystem::rename(jobFile, workingFile); // Throws filesystem::filesystem_error on error
	std::cout << "Moved job to working directory: " << jobFile << "=>" << workingFile << std::endl;

	std::ifstream jobIn(workingFile, std::ios::binary);

	unsigned int fileDedekindTarget = deserializeU32(jobIn);
	if(dedekindNumberProject != fileDedekindTarget) {
		std::cerr << "File for incorrect Dedekind Target! Specified Target: " << dedekindNumberProject << ", target from file: " << fileDedekindTarget << std::endl;
		std::abort();
	}

	std::vector<std::uint32_t> topIndices = deserializePODVector<std::uint32_t>(jobIn);
	checkFileDone(jobIn);

	return topIndices;
}

void saveResults(unsigned int dedekindNumberProject, const std::string& computeFolder, int jobIndex, const std::string& computeID, const std::vector<BetaResult>& betaResults) {
	std::string workingFile = computeFilePath(computeFolder, "working", jobIndex, "_" + computeID + ".job");
	std::string finishedFile = computeFilePath(computeFolder, "finished", jobIndex, "_" + computeID + ".job");
	std::string resultsFile = computeFilePath(computeFolder, "results", jobIndex, "_" + computeID + ".results");

	if(std::filesystem::exists(finishedFile)) {
		std::cerr << "Results file (" << finishedFile << ") already exists! Job " << jobIndex << " has already terminated!";
		std::abort();
	}
	if(std::filesystem::exists(resultsFile)) {
		std::cerr << "Results file (" << resultsFile << ") already exists! Job " << jobIndex << " has already terminated!";
		std::abort();
	}
	
	{
		std::ofstream resultsOut(resultsFile, std::ios::binary);
		serializeU32(dedekindNumberProject, resultsOut);
		serializePODVector(betaResults, resultsOut);
	}
	std::filesystem::rename(workingFile, finishedFile); // Throws filesystem::filesystem_error on error
	// At this point the job is committed and finished!
}

std::vector<BetaResult> readResultsFile(unsigned int dedekindNumberProject, const std::filesystem::path& filePath) {
	std::ifstream resultFileIn(filePath, std::ios::binary);

	unsigned int fileDedekindTarget = deserializeU32(resultFileIn);
	if(dedekindNumberProject != fileDedekindTarget) {
		std::cerr << "Results File for incorrect Dedekind Target! Specified Target: " << dedekindNumberProject << ", target from file: " << fileDedekindTarget << std::endl;
		std::abort();
	}

	std::vector<BetaResult> result = deserializePODVector<BetaResult>(resultFileIn);
	checkFileDone(resultFileIn);
	return result;
}

std::vector<BetaSum> collectAllResultFiles(const std::string& computeFolder, unsigned int dedekindNumberProject) {
	BetaResultCollector collector(dedekindNumberProject - 2);

	std::string resultsDirectory = computeFolderPath(computeFolder, "results");
	for(const auto& file : std::filesystem::directory_iterator(resultsDirectory)) {
		std::filesystem::path path = file.path();
		std::string filePath = path.string();
		if(file.is_regular_file() && filePath.substr(filePath.find_last_of(".")) == ".results") {
			// Results file
			std::cout << "Reading results file " << filePath << std::endl;
			std::vector<BetaResult> results = readResultsFile(dedekindNumberProject, path);
			collector.addBetaResults(results);
		}
	}

	return collector.getResultingSums();
}


void processJob(unsigned int Variables, const std::string& computeFolder, int jobIndex, const std::string& methodName, void (*processorFunc)(PCoeffProcessingContext&)) {
	std::string computeID = methodName + "_" + getComputeIdentifier();
	std::vector<std::uint32_t> topsToProcess = loadJob(Variables + 2, computeFolder, computeID, jobIndex);

	std::cout << "Starting Computation..." << std::endl;
	std::vector<BetaResult> betaResults = pcoeffPipeline(Variables, topsToProcess, processorFunc, 300, 5, 50);

	
	if(betaResults.size() != topsToProcess.size()) {
		std::cerr << "This can't happen! Number of tops processed isn't number of inputs tops!" << std::endl;
		std::abort();
	}

	std::cout << "Original top indices: " << std::endl;
	for(unsigned int idx : topsToProcess) {
		std::cout << idx << ", ";
	}
	std::cout << std::endl;

	std::cout << "Result indices: " << std::endl;
	for(BetaResult& r : betaResults) {
		std::cout << r.topIndex << ", ";
	}
	std::cout << std::endl;

	saveResults(Variables + 2, computeFolder, jobIndex, computeID, betaResults);
}

