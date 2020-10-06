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

std::vector<std::vector<SymmetryGroup>> findAllSymmetryGroups(const std::vector<FunctionInput>& available) {
	std::vector<std::vector<SymmetryGroup>> result(available.size() + 1);

	result[0] = std::vector<SymmetryGroup>{SymmetryGroup{1, EquivalenceClass::emptyEquivalenceClass}};
	for(size_t groupSize = 1; groupSize < available.size(); groupSize++) {
		result[groupSize] = findSymmetryGroups(available, groupSize);
	}
	result[available.size()] = std::vector<SymmetryGroup>{SymmetryGroup{1, EquivalenceClass(preprocess(available))}};
	return result;
}

std::vector<std::vector<SymmetryGroup>> findAllSymmetryGroupsFast(const std::vector<FunctionInput>& available) {
	std::vector<std::vector<SymmetryGroup>> foundGroups(available.size() + 1);

	foundGroups[0] = std::vector<SymmetryGroup>{SymmetryGroup{1, EquivalenceClass::emptyEquivalenceClass}}; // equivalence classes of size 0, only one
	foundGroups[1] = std::vector<SymmetryGroup>{SymmetryGroup{available.size(), EquivalenceClass(preprocess(std::vector<FunctionInput>{available[0]}))}}; // equivalence classes of size 1, only one

	std::vector<size_t> lastStartIndices{1};
	for(int i = 2; i < available.size(); i++) {
		std::vector<SymmetryGroup> groupsForThisSize;
		// try to extend each of the previous groups by 1

	}
	foundGroups[available.size()] = std::vector<SymmetryGroup>{SymmetryGroup{1, EquivalenceClass(preprocess(available))}};

	/*forEachSubgroup(available, groupSize, [&foundGroups](const std::vector<FunctionInput>& subGroup) {
		PreprocessedFunctionInputSet normalized = preprocess(subGroup);
		for(SymmetryGroup& s : foundGroups) {
			if(isIsomorphic(s.example, normalized)) {
				s.count++;
				return; // don't add new symmetryGroup
			}
		}
		// not found => add new symmetryGroup
		foundGroups.push_back(SymmetryGroup{1, EquivalenceClass{normalized, hash(normalized.functionInputSet)}});
	});*/

	return foundGroups;
}
