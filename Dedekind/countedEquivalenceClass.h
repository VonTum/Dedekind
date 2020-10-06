#pragma once

#include <vector>

#include "functionInput.h"
#include "equivalenceClass.h"



struct SymmetryGroup {
	int64_t count;
	EquivalenceClass example;
};

std::vector<SymmetryGroup> findSymmetryGroups(const std::vector<FunctionInput>& available, size_t groupSize) {
	std::vector<SymmetryGroup> foundGroups;

	forEachSubgroup(available, groupSize, [&foundGroups](const std::vector<FunctionInput>& subGroup) {
		PreprocessedFunctionInputSet preprocessed = preprocess(subGroup);
		for(SymmetryGroup& s : foundGroups) {
			if(s.example.contains(preprocessed)) {
				s.count++;
				return; // don't add new symmetryGroup
			}
		}
		// not found => add new symmetryGroup
		foundGroups.push_back(SymmetryGroup{1, EquivalenceClass(preprocessed)});
	});

	return foundGroups;
}

