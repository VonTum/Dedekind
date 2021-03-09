#pragma once

#include <string>

template<unsigned int Variables>
std::string allMBFSSorted() {
	std::string name = "../data/allUniqueMBFSorted";
	name.append(std::to_string(Variables));
	name.append(".mbf");
	return name;
}

template<unsigned int Variables>
std::string allIntervals() {
	std::string name = "../data/allIntervals";
	name.append(std::to_string(Variables));
	name.append(".intervals");
	return name;
}
