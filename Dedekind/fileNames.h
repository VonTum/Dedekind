#pragma once

#include <string>

namespace FileName {
inline std::string allMBFSInfo(unsigned int Variables) {
	std::string name = "../data/allUniqueMBF";
	name.append(std::to_string(Variables));
	name.append("info.txt");
	return name;
}

inline std::string allMBFS(unsigned int Variables) {
	std::string name = "../data/allUniqueMBF";
	name.append(std::to_string(Variables));
	name.append(".mbf");
	return name;
}

inline std::string mbfLinks(unsigned int Variables) {
	std::string name = "../data/mbfLinks";
	name.append(std::to_string(Variables));
	name.append(".mbfLinks");
	return name;
}

inline std::string linkStats(unsigned int Variables) {
	std::string name = "../data/linkStats";
	name.append(std::to_string(Variables));
	name.append(".txt");
	return name;
}

inline std::string intervalStats(unsigned int Variables) {
	std::string name = "../data/intervalStats";
	name.append(std::to_string(Variables));
	name.append(".txt");
	return name;
}

inline std::string allMBFSSorted(unsigned int Variables) {
	std::string name = "../data/allUniqueMBFSorted";
	name.append(std::to_string(Variables));
	name.append(".mbf");
	return name;
}

inline std::string allIntervals(unsigned int Variables) {
	std::string name = "../data/allIntervals";
	name.append(std::to_string(Variables));
	name.append(".intervals");
	return name;
}
};
