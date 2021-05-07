#pragma once

#include <string>

namespace FileName {
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
};
