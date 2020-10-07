#pragma once

#include <vector>

#include "set.h"
#include "functionInput.h"
#include "equivalenceClass.h"


struct SymmetryGroup {
	int64_t count;
	EquivalenceClass example;
};

bool allUnique(const std::vector<SymmetryGroup>& groups) {
	for(size_t i = 0; i < groups.size() - 1; i++) {
		for(size_t j = i + 1; j < groups.size(); j++) {
			auto& a = groups[i].example;
			auto& b = groups[j].example;
			if(a.contains(b) || b.contains(a)) return false;
		}
	}
	return true;
}

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

#define DEBUG_PRINT(v) std::cout << v

std::vector<std::vector<SymmetryGroup>> findAllSymmetryGroupsFast(const std::vector<FunctionInput>& available) {
	std::vector<std::vector<SymmetryGroup>> foundGroups(available.size() + 1);

	foundGroups[0] = std::vector<SymmetryGroup>{SymmetryGroup{1, EquivalenceClass::emptyEquivalenceClass}}; // equivalence classes of size 0, only one
	EquivalenceClass classOfOneEnabled(preprocess(std::vector<FunctionInput>{available[0]}));
	foundGroups[1] = std::vector<SymmetryGroup>{SymmetryGroup{int64_t(available.size()), classOfOneEnabled}}; // equivalence classes of size 1, only one

	std::vector<std::pair<set<FunctionInput>, set<FunctionInput>>> prevLeftOverItems(1);
	prevLeftOverItems[0].first.push_back(available[0]);
	for(size_t i = 1; i < available.size(); i++) {
		prevLeftOverItems[0].second.push_back(available[i]);
	}
	for(size_t groupSize = 2; groupSize < available.size(); groupSize++) {
		DEBUG_PRINT("Looking at size " << groupSize << '/' << available.size());
		const std::vector<SymmetryGroup>& prevGroups = foundGroups[groupSize - 1];
		std::vector<SymmetryGroup>& curGroups = foundGroups[groupSize];
		std::vector<std::pair<set<FunctionInput>, set<FunctionInput>>> curLeftOverItems;
		// try to extend each of the previous groups by 1
		for(size_t groupIndex = 0; groupIndex < prevGroups.size(); groupIndex++) {
			const SymmetryGroup& g = prevGroups[groupIndex];
			const std::pair<set<FunctionInput>, set<FunctionInput>>& leftOverItems = prevLeftOverItems[groupIndex];
			std::vector<FunctionInput> potentialNewClass(leftOverItems.first.size() + 1);
			for(size_t i = 0; i < leftOverItems.first.size(); i++) {
				potentialNewClass[i] = leftOverItems.first[i];
			}
			for(size_t indexInLeftOverItems = 0; indexInLeftOverItems < leftOverItems.second.size(); indexInLeftOverItems++) {
				potentialNewClass[leftOverItems.first.size()] = leftOverItems.second[indexInLeftOverItems];
				PreprocessedFunctionInputSet preprocessed = preprocess(potentialNewClass);
				for(SymmetryGroup& newGroup : curGroups) {
					if(newGroup.example.contains(preprocessed)) {
						newGroup.count += g.count;
						goto wasAddedToExisting;
					}
				}
				// could not find group this is isomorphic to, add as new symmetry group
				curGroups.push_back(SymmetryGroup{g.count, EquivalenceClass(preprocessed)});
				{
					set<FunctionInput> resultingNewClass(potentialNewClass.size());
					for(size_t i = 0; i < potentialNewClass.size(); i++) {
						resultingNewClass[i] = potentialNewClass[i];
					}
					curLeftOverItems.emplace_back(resultingNewClass, leftOverItems.second.withIndexRemoved(indexInLeftOverItems));
				}
				wasAddedToExisting:;
			}
		}
		// all groups covered, add new groups to list, apply correction and repeat
		prevLeftOverItems = std::move(curLeftOverItems);
		assert(allUnique(curGroups));
		for(SymmetryGroup& newGroup : curGroups) {
			//assert(newGroup.count % groupSize == 0);
			newGroup.count /= groupSize;
		}
		DEBUG_PRINT(" done! " << curGroups.size() << " groups found!" << std::endl);
	}
	foundGroups[available.size()] = std::vector<SymmetryGroup>{SymmetryGroup{1, EquivalenceClass(preprocess(available))}};

	return foundGroups;
}
