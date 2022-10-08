#pragma once

#include <cstdint>
#include <fstream>

#include "fileNames.h"
#include "knownData.h"

#include "flatMBFStructure.h"

#include "flatPCoeffProcessing.h"

#include "serialization.h"
#include "threadPool.h"

// Creates all necessary files and folders for a project to compute the given dedekind number
// Requires that the compute folder does not already exist to prevent data loss
void initializeComputeProject(unsigned int Variables, std::string computeFolder, size_t numberOfJobs, size_t topsPerBatch);

// Creates the necessary validation files
void initializeValidationFiles(unsigned int Variables, std::string computeFolder, const std::vector<std::string>& computeIDs);

void processJob(unsigned int Variables, const std::string& computeFolder, const std::string& jobID, const std::string& methodName, void (*processorFunc)(PCoeffProcessingContext&, const void*[2]), void(*validator)(const OutputBuffer&, const void*, ThreadPool&) = nullptr);

std::vector<BetaSumPair> collectAllResultFiles(unsigned int Variables, const std::string& computeFolder);
std::unique_ptr<ValidationData[]> collectAllValidationFiles(unsigned int Variables, const std::string& computeFolder);

template<unsigned int Variables>
void collectAndProcessResults(const std::string& computeFolder) {
	std::vector<BetaSumPair> allResults = collectAllResultFiles(Variables, computeFolder);
	std::unique_ptr<ValidationData[]> completeValidationBuffer = collectAllValidationFiles(Variables, computeFolder);

	computeFinalDedekindNumberFromGatheredResults<Variables>(allResults, completeValidationBuffer.get());
}
