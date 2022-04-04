#pragma once

#include <string>

namespace FileName {
extern std::string dataPath;
void setDataPath(std::string dataPath);

std::string allMBFSInfo(unsigned int Variables);
std::string allMBFS(unsigned int Variables);
std::string mbfLinks(unsigned int Variables);
std::string linkStats(unsigned int Variables);
std::string intervalStats(unsigned int Variables);
std::string allMBFSSorted(unsigned int Variables);
std::string allIntervals(unsigned int Variables);
std::string allIntervalSymmetries(unsigned int Variables);
std::string benchmarkSet(unsigned int Variables);
std::string benchmarkSetTopBots(unsigned int Variables);

// Test sets for the hardware accelerator
std::string pipelineTestSet(unsigned int Variables);
std::string pipeline6PackTestSet(unsigned int Variables);
std::string pipeline24PackTestSet(unsigned int Variables);
std::string permuteCheck24TestSet(unsigned int Variables);
std::string pipeline6PackTestSetForOpenCLMem(unsigned int Variables);
std::string pipeline6PackTestSetForOpenCLCpp(unsigned int Variables);
std::string pipeline24PackTestSetForOpenCLMem(unsigned int Variables);
std::string pipeline24PackTestSetForOpenCLCpp(unsigned int Variables);
std::string FullPermutePipelineTestSetOpenCLMem(unsigned int Variables);
std::string FullPermutePipelineTestSetOpenCLCpp(unsigned int Variables);
std::string singleStreamPipelineTestSetForOpenCLMem(unsigned int Variables);


// File names for the flat MBF structure
std::string flatMBFs(unsigned int Variables);
std::string flatClassInfo(unsigned int Variables);
std::string flatNodes(unsigned int Variables);
std::string flatLinks(unsigned int Variables);
std::string flatMBFsU64(unsigned int Variables);
};
