#pragma once

#include <cstdint>
#include <fstream>

#include "fileNames.h"
#include "knownData.h"

#include "flatMBFStructure.h"

#include "flatPCoeffProcessing.h"

#include "serialization.h"


ValidationData getIntactnessCheckSum(const ValidationData* buf, unsigned int Variables);

// Creates all necessary files and folders for a project to compute the given dedekind number
// Requires that the compute folder does not already exist to prevent data loss
void initializeComputeProject(unsigned int Variables, std::string computeFolder, size_t numberOfJobs, size_t numberOfJobsToActuallyGenerate);

void writeProcessingBufferPairToFile(const char* fileName, const OutputBuffer& outBuf);
uint64_t readProcessingBufferPairFromFile(const char* fileName, NodeIndex* idxBuf = nullptr, ProcessedPCoeffSum* resultsBuf = nullptr);

// Returns true if computation finished without detected errors
bool processJob(unsigned int Variables, const std::string& computeFolder, const std::string& jobID, const std::string& methodName, void (*processorFunc)(PCoeffProcessingContext&), void*(*validator)(void*) = nullptr);

std::vector<BetaResult> readResultsFile(unsigned int Variables, const char* filePath, ValidationData& checkSum);

BetaResultCollector collectAllResultFilesAndRecoverFailures(unsigned int Variables, const std::string& computeFolder);

void resetUnfinishedJobs(const std::string& computeFolder, const std::string& folderName, size_t jobLowerBound, size_t jobUpperBound);

void collectAndProcessResults(unsigned int Variables, const std::string& computeFolder);

void checkProjectResultsIdentical(unsigned int Variables, const std::string& computeFolderA, const std::string& computeFolderB);

void checkProjectIntegrity(unsigned int Variables, const std::string& computeFolder);
