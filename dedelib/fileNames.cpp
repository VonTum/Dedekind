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

std::string allMBFSInfo(unsigned int Variables) {
	std::string name = dataPath + "allUniqueMBF";
	name.append(std::to_string(Variables));
	name.append("info.txt");
	return name;
}

std::string allMBFS(unsigned int Variables) {
	std::string name = dataPath + "allUniqueMBF";
	name.append(std::to_string(Variables));
	name.append(".mbf");
	return name;
}

std::string mbfLinks(unsigned int Variables) {
	std::string name = dataPath + "mbfLinks";
	name.append(std::to_string(Variables));
	name.append(".mbfLinks");
	return name;
}

std::string linkStats(unsigned int Variables) {
	std::string name = dataPath + "linkStats";
	name.append(std::to_string(Variables));
	name.append(".txt");
	return name;
}

std::string intervalStats(unsigned int Variables) {
	std::string name = dataPath + "intervalStats";
	name.append(std::to_string(Variables));
	name.append(".txt");
	return name;
}

std::string allMBFSSorted(unsigned int Variables) {
	std::string name = dataPath + "allUniqueMBFSorted";
	name.append(std::to_string(Variables));
	name.append(".mbf");
	return name;
}

std::string allIntervals(unsigned int Variables) {
	std::string name = dataPath + "allIntervals";
	name.append(std::to_string(Variables));
	name.append(".intervals");
	return name;
}

std::string allIntervalSymmetries(unsigned int Variables) {
	std::string name = dataPath + "allIntervalSymmetries";
	name.append(std::to_string(Variables));
	name.append(".intervalSymmetries");
	return name;
}

std::string benchmarkSet(unsigned int Variables) {
	std::string name = dataPath + "benchmarkSet";
	name.append(std::to_string(Variables));
	name.append(".intervalSymmetries");
	return name;
}

std::string benchmarkSetTopBots(unsigned int Variables) {
	std::string name = dataPath + "benchmarkSet";
	name.append(std::to_string(Variables));
	name.append(".topBots");
	return name;
}
};
