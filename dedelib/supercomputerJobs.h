#pragma once

#include <cstdint>
#include <fstream>

#include "fileNames.h"
#include "knownData.h"

#include "flatMBFStructure.h"

#include "flatPCoeffProcessing.h"

#include "serialization.h"


struct ValidationFileData{
	unsigned int Variables;
	std::unique_ptr<char[]> memory;
	ValidationData* savedValidationBuffer;
	uint8_t* savedTopsBitset;

	ValidationFileData() = default;
	ValidationFileData(unsigned int Variables);
	size_t getTotalMemorySize() const;
	void initializeZero();
	void readFromFile(int validationFD);
	void writeToFile(int validationFD) const;
	bool isTopPresent(NodeIndex topIdx) const;
	void checkHasAllTops() const;
	void addTop(NodeIndex topIdx);
	// Returns the number of tops added
	size_t mergeIntoThis(const ValidationFileData& other);
};

bool operator==(const ValidationFileData& a, const ValidationFileData& b);
inline bool operator!=(const ValidationFileData& a, const ValidationFileData& b) {
	return !(a == b);
}
 
// Creates all necessary files and folders for a project to compute the given dedekind number
// Requires that the compute folder does not already exist to prevent data loss
void initializeComputeProject(unsigned int Variables, std::string computeFolder, size_t numberOfJobs, size_t numberOfJobsToActuallyGenerate);

// Creates the necessary validation files
void initializeValidationFiles(unsigned int Variables, std::string computeFolder, const std::vector<std::string>& computeIDs);

void processJob(unsigned int Variables, const std::string& computeFolder, const std::string& jobID, const std::string& methodName, void (*processorFunc)(PCoeffProcessingContext&), void*(*validator)(void*) = nullptr);

void resetUnfinishedJobs(const std::string& computeFolder);


BetaResultCollector collectAllResultFiles(unsigned int Variables, const std::string& computeFolder);
ValidationFileData collectAllValidationFiles(unsigned int Variables, const std::string& computeFolder);

template<unsigned int Variables>
void collectAndProcessResults(const std::string& computeFolder) {
	BetaResultCollector allResults = collectAllResultFiles(Variables, computeFolder);
	ValidationFileData completeValidationBuffer = collectAllValidationFiles(Variables, computeFolder);
	completeValidationBuffer.checkHasAllTops();

	computeFinalDedekindNumberFromGatheredResults<Variables>(allResults.getResultingSums(), completeValidationBuffer.savedValidationBuffer);
}

void checkProjectResultsIdentical(unsigned int Variables, const std::string& computeFolderA, const std::string& computeFolderB);
