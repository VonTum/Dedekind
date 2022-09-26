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

void processJob(unsigned int Variables, const std::string& computeFolder, int jobIndex, const std::string& methodName, void (*processorFunc)(PCoeffProcessingContext&, const void*[2]));

std::vector<BetaSumPair> collectAllResultFiles(const std::string& computeFolder, unsigned int dedekindNumberProject);

template<unsigned int Variables>
void collectAndProcessResults(const std::string& computeFolder) {
	std::vector<BetaSumPair> allResults = collectAllResultFiles(computeFolder, Variables + 2);

	std::cout << "Reading FlatMBFStructure..." << std::endl;
	const FlatMBFStructure<Variables> allMBFData = readFlatMBFStructure<Variables>();
	std::cout << "FlatMBFStructure initialized." << std::endl;

	std::cout << "Computation finished." << std::endl;
	u192 dedekindNumber = computeDedekindNumberFromBetaSums(allMBFData, allResults);

	std::cout << "D(" << (Variables + 2) << ") = " << dedekindNumber << std::endl;
}
