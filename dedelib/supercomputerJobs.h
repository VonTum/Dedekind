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

void processJob(unsigned int Variables, const std::string& computeFolder, const std::string& jobID, const std::string& methodName, void (*processorFunc)(PCoeffProcessingContext&), void*(*validator)(void*) = nullptr);

void resetUnfinishedJobs(const std::string& computeFolder);

void collectAndProcessResults(unsigned int Variables, const std::string& computeFolder);

void checkProjectResultsIdentical(unsigned int Variables, const std::string& computeFolderA, const std::string& computeFolderB);

void checkProjectIntegrity(unsigned int Variables, const std::string& computeFolder);
