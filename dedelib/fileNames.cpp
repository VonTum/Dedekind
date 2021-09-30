#include "fileNames.h"

#include <iostream>

namespace FileName {

static std::string dataPath = "../data/";

void setDataPath(std::string newPath) {
	std::cout << "Set base path: " << newPath.c_str() << std::endl;
	if(newPath.empty()) {
		dataPath = "";
	} else if(*newPath.rbegin() != '/') {
		dataPath = newPath + "/";
	} else {
		dataPath = newPath;
	}
}

static std::string makeBasicName(unsigned int Variables, const char* id, const char* suffix) {
	std::string name = dataPath + id;
	name.append(std::to_string(Variables));
	name.append(suffix);
	return name;
}

std::string allMBFSInfo(unsigned int Variables) {
	return makeBasicName(Variables, "allUniqueMBF", "info.txt");
}

std::string allMBFS(unsigned int Variables) {
	return makeBasicName(Variables, "allUniqueMBF", ".mbf");
}

std::string mbfLinks(unsigned int Variables) {
	return makeBasicName(Variables, "mbfLinks", ".mbfLinks");
}

std::string linkStats(unsigned int Variables) {
	return makeBasicName(Variables, "linkStats", ".txt");
}

std::string intervalStats(unsigned int Variables) {
	return makeBasicName(Variables, "intervalStats", ".txt");
}

std::string allMBFSSorted(unsigned int Variables) {
	return makeBasicName(Variables, "allUniqueMBFSorted", ".mbf");
}

std::string allIntervals(unsigned int Variables) {
	return makeBasicName(Variables, "allIntervals", ".intervals");
}

std::string allIntervalSymmetries(unsigned int Variables) {
	return makeBasicName(Variables, "allIntervalSymmetries", ".intervalSymmetries");
}

std::string benchmarkSet(unsigned int Variables) {
	return makeBasicName(Variables, "benchmarkSet", ".intervalSymmetries");
}

std::string benchmarkSetTopBots(unsigned int Variables) {
	return makeBasicName(Variables, "benchmarkSet", ".topBots");
}

std::string pipelineTestSet(unsigned int Variables) {
	return makeBasicName(Variables, "pipelineTestSet", ".mem");
}

// File names for the flat MBF structure
std::string flatMBFs(unsigned int Variables) {
	return makeBasicName(Variables, "flatMBFs", ".mbf");
}
std::string flatClassInfo(unsigned int Variables) {
	return makeBasicName(Variables, "flatClassInfo", ".classInfo");
}
std::string flatNodes(unsigned int Variables) {
	return makeBasicName(Variables, "flatNodes", ".flatNodes");
}
std::string flatLinks(unsigned int Variables) {
	return makeBasicName(Variables, "flatLinks", ".flatLinks");
}
};
