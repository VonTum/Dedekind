#pragma once

#include <cstdint>
#include <fstream>

#include "fileNames.h"
#include "knownData.h"

#include "flatMBFStructure.h"

#include "flatPCoeffProcessing.h"

#include "serialization.h"

// Creates all necessary files and folders for a project to compute the given dedekind number
// Requires that the compute folder does not already exist to prevent data loss
void initializeComputeProject(std::string computeFolder, unsigned int dedekindNumber, size_t numberOfJobs, size_t topsPerBatch);

std::string getComputeIdentifier();
std::vector<std::uint32_t> loadJob(unsigned int dedekindNumberProject, const std::string& computeFolder, const std::string& computeID, int jobIndex);
void saveResults(unsigned int dedekindNumberProject, const std::string& computeFolder, int jobIndex, const std::string& computeID, const std::vector<BetaResult>& betaResults);

template<unsigned int Variables, size_t BatchSize, typename Processor>
void processJob(const std::string& computeFolder, int jobIndex, const std::string& methodName, Processor processorFunc) {
	std::string computeID = methodName + "_" + getComputeIdentifier();
	std::vector<std::uint32_t> topsToProcess = loadJob(Variables + 2, computeFolder, computeID, jobIndex);

	std::cout << "Reading FlatMBFStructure..." << std::endl;
	const FlatMBFStructure<Variables> allMBFData = readFlatMBFStructure<Variables>();
	std::cout << "FlatMBFStructure initialized." << std::endl;

	std::cout << "Starting Computation..." << std::endl;
	std::vector<BetaResult> betaResults = pcoeffPipeline<Variables, BatchSize, Processor>(allMBFData, topsToProcess, processorFunc, 300, 5);

	
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

std::vector<BetaSum> collectAllResultFiles(const std::string& computeFolder, unsigned int dedekindNumberProject);

template<unsigned int Variables>
void collectAndProcessResults(const std::string& computeFolder) {
	std::vector<BetaSum> allResults = collectAllResultFiles(computeFolder, Variables + 2);

	std::cout << "Reading FlatMBFStructure..." << std::endl;
	const FlatMBFStructure<Variables> allMBFData = readFlatMBFStructure<Variables>();
	std::cout << "FlatMBFStructure initialized." << std::endl;

	std::cout << "Computation finished." << std::endl;
	u192 dedekindNumber = computeDedekindNumberFromBetaSums(allMBFData, allResults);

	std::cout << "D(" << (Variables + 2) << ") = " << dedekindNumber << std::endl;
}
