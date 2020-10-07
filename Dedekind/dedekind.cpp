
#include <iostream>
#include <cassert>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <algorithm>
#include <chrono>

#include "countedEquivalenceClass.h"
#include "layerStack.h"
#include "toString.h"




typedef int64_t bigInt;
typedef int64_t countInt;


struct SymmetryGroupValue : public SymmetryGroup {
	bigInt value; // extra value added for each instance of this symmetryGroup

	SymmetryGroupValue() = default;
	SymmetryGroupValue(const SymmetryGroup& sg, bigInt value) : SymmetryGroup(sg), value(value) {}
	SymmetryGroupValue(SymmetryGroup&& sg, bigInt value) : SymmetryGroup(std::move(sg)), value(value) {}
};

std::ostream& operator<<(std::ostream& os, const SymmetryGroupValue& s) {
	os << s.example << ':' << s.count << '$' << s.value;
	return os;
}

std::vector<countInt> countEachSymmetryGroup(const std::vector<FunctionInput>& available, size_t groupSize, const std::vector<SymmetryGroupValue>& groups) {
	std::vector<countInt> result(groups.size(), 0);
	forEachSubgroup(available, groupSize, [&groups, &result](const std::vector<FunctionInput>& subGroup) {
		PreprocessedFunctionInputSet preprocessed = preprocess(subGroup);
		for(size_t i = 0; i < groups.size(); i++){
			if(groups[i].example.contains(preprocessed)) {
				result[i]++;
				return; // found group
			}
		}
		// not found => error: group must exist
		throw "error group does not exist";
	});
	return result;
}

bigInt computeTotalOptionsForAvailableChoices(const std::vector<FunctionInput>& availableOptions, const std::vector<std::vector<SymmetryGroupValue>>& symmetryGroups) {
	bigInt totalOptions = 1; // 1 for everything on, no further options

	for(size_t offCount = 1; offCount <= availableOptions.size(); offCount++) {
		const std::vector<SymmetryGroupValue>& relevantSymmetryGroupsOfLayerAbove = symmetryGroups[offCount];
		std::vector<countInt> counts = countEachSymmetryGroup(availableOptions, offCount, relevantSymmetryGroupsOfLayerAbove);

		for(size_t i = 0; i < relevantSymmetryGroupsOfLayerAbove.size(); i++) {
			totalOptions += counts[i] * relevantSymmetryGroupsOfLayerAbove[i].value;
		}
	}

	return totalOptions;
}

bigInt computeTotalOptionsForAvailableChoicesFast(const std::vector<FunctionInput>& availableOptions, const std::vector<std::vector<SymmetryGroupValue>>& symmetryGroups) {
	bigInt totalOptions = 1; // 1 for everything on, no further options

	if(availableOptions.size() == 0) return 1;
	std::vector<std::vector<SymmetryGroup>> symGroupsOfAvailableOptions = findAllSymmetryGroupsFast(availableOptions);

	for(size_t offCount = 1; offCount <= availableOptions.size(); offCount++) {
		const std::vector<SymmetryGroupValue>& relevantSymmetryGroupsOfLayerAbove = symmetryGroups[offCount];
		
		const std::vector<SymmetryGroup>& relevantSymmetryGroupsOfAvailable = symGroupsOfAvailableOptions[offCount];


		for(size_t i = 0; i < relevantSymmetryGroupsOfAvailable.size(); i++) {
			const SymmetryGroup& g = relevantSymmetryGroupsOfAvailable[i];
			for(size_t j = 0; j < relevantSymmetryGroupsOfLayerAbove.size(); j++) {
				const SymmetryGroupValue& sgv = relevantSymmetryGroupsOfLayerAbove[j];
				if(sgv.example.contains(g.example)) {
					totalOptions += g.count * sgv.value;
				}
			}
			throw "g does not exist in sgv";
		}
	}

	return totalOptions;
}

std::vector<FunctionInput> computeAvailableElementsInLayerAbove(const std::vector<FunctionInput>& offInputSet, const std::vector<FunctionInput>& curLayer, const std::vector<FunctionInput>& layerAbove) {
	std::vector<FunctionInput> onInputSet = removeAll(curLayer, offInputSet);
	return removeSuperInputs(layerAbove, onInputSet);
}


std::vector<SymmetryGroupValue> emptyGroupValueList{SymmetryGroupValue(SymmetryGroup{1, EquivalenceClass::emptyEquivalenceClass}, 1)};

bigInt getTotalOfAllSymmetryGroups(std::vector<std::vector<SymmetryGroupValue>>& valuesOfLayerAbove) {
	bigInt totalValue = 0;
	for(const std::vector<SymmetryGroupValue>& lst : valuesOfLayerAbove) {
		for(const SymmetryGroupValue& v : lst) {
			totalValue += v.count * v.value;
		}
	}
	return totalValue;
}

std::vector<std::vector<SymmetryGroupValue>> computeSymmetryGroupValues(const LayerStack& stack, size_t layerIndex) {
	if(layerIndex == stack.layers.size() - 1) {
		return std::vector<std::vector<SymmetryGroupValue>>{emptyGroupValueList, std::vector<SymmetryGroupValue>{SymmetryGroupValue(SymmetryGroup{1, EquivalenceClass(preprocess(stack.layers.back()))}, 1)}};
	}
	std::vector<std::vector<SymmetryGroupValue>> valuesOfLayerAbove = computeSymmetryGroupValues(stack, layerIndex + 1);

	const std::vector<FunctionInput>& curLayer = stack.layers[layerIndex];
	const std::vector<FunctionInput>& layerAbove = stack.layers[layerIndex+1];
	std::vector<std::vector<SymmetryGroup>> symmetryGroupsOfCurLayer = findAllSymmetryGroupsFast(curLayer);
	std::vector<std::vector<SymmetryGroupValue>> symmetryGroupsOfThisLayer(curLayer.size()+1);
	symmetryGroupsOfThisLayer[0] = emptyGroupValueList;
	symmetryGroupsOfThisLayer[curLayer.size()] = std::vector<SymmetryGroupValue>{SymmetryGroupValue(symmetryGroupsOfCurLayer[curLayer.size()][0], getTotalOfAllSymmetryGroups(valuesOfLayerAbove))};
	for(size_t groupSize = 1; groupSize < curLayer.size(); groupSize++) {
		const std::vector<SymmetryGroup>& symmetryGroupsForGroupSize = symmetryGroupsOfCurLayer[groupSize];
		std::vector<SymmetryGroupValue> symmetryGroupValueAssociationsOfThisLayer;
		symmetryGroupValueAssociationsOfThisLayer.reserve(symmetryGroupsForGroupSize.size());
		for(const SymmetryGroup& sg : symmetryGroupsForGroupSize) {
			std::vector<FunctionInput> availableElementsInLayerAbove = computeAvailableElementsInLayerAbove(sg.example.functionInputSet, curLayer, layerAbove);

			bigInt valueOfSG = computeTotalOptionsForAvailableChoices(availableElementsInLayerAbove, valuesOfLayerAbove);

			symmetryGroupValueAssociationsOfThisLayer.emplace_back(sg, valueOfSG);
		}
		symmetryGroupsOfThisLayer[groupSize] = symmetryGroupValueAssociationsOfThisLayer;
	}
	return symmetryGroupsOfThisLayer;
}

struct ValueSharedSymmetryGroupValue {
	std::vector<SymmetryGroup> groups;
	bigInt value;

	ValueSharedSymmetryGroupValue() = default;
	ValueSharedSymmetryGroupValue(const SymmetryGroupValue& sgv) : groups{sgv}, value(sgv.value){}
};

std::vector<ValueSharedSymmetryGroupValue> groupByValue(const std::vector<SymmetryGroupValue>& sgs) {
	std::vector<ValueSharedSymmetryGroupValue> foundGroups;

	for(const SymmetryGroupValue& sgv : sgs) {
		for(ValueSharedSymmetryGroupValue& possibleGroup : foundGroups) {
			if(possibleGroup.value == sgv.value) {
				possibleGroup.groups.push_back(sgv);
				goto next;
			}
		}
		foundGroups.emplace_back(sgv);
		next:;
	}

	return foundGroups;
}

void printSymmetryGroupsForInputSetAndGroupSize(const std::vector<FunctionInput>& inputSet, size_t groupSize) {
	auto sgs = findSymmetryGroups(inputSet, groupSize);
	//auto groupsByValue = groupByValue(sgs);
	std::cout << "Size " << groupSize << " => " << sgs.size() << " groups" << std::endl << sgs << std::endl;
}

void printAllSymmetryGroupsForInputSet(const std::vector<FunctionInput>& inputSet) {
	auto allSets = findAllSymmetryGroups(inputSet);
	std::cout << "Symmetry groups for: " << std::endl << inputSet << std::endl;
	for(size_t size = 0; size <= inputSet.size(); size++) {
		std::cout << "Size " << size << " => " << allSets[size].size() << " groups" << std::endl << allSets[size] << std::endl;
	}
}
void printAllSymmetryGroupsForInputSetFast(const std::vector<FunctionInput>& inputSet) {
	auto allSets = findAllSymmetryGroupsFast(inputSet);
	std::cout << "Symmetry groups for: " << std::endl << inputSet << std::endl;
	for(size_t size = 0; size <= inputSet.size(); size++) {
		std::cout << "Size " << size << " => " << allSets[size].size() << " groups" << std::endl << allSets[size] << std::endl;
	}
}

bigInt dedekind(int order) {
	std::cout << "dedekind(" << order << ") = ";
	LayerStack layers = generateLayers(order);

	auto lastLayerValues = computeSymmetryGroupValues(layers, 0);

	bigInt result = lastLayerValues[1][0].value + 1;

	std::cout << result << std::endl;

	return result;
}

int main() {

	/*dedekind(1);
	dedekind(2);
	dedekind(3);
	dedekind(4);
	dedekind(5);
	dedekind(6);
	dedekind(7);
	
	return 0;//*/

	/*LayerStack layers = generateLayers(4);
	std::cout << layers << std::endl;*/

	std::cout << "Starting" << std::endl;
	LayerStack layers = generateLayers(6);
	std::cout << "Layers made" << std::endl;

	/*std::cout << layers << std::endl;
	
	std::vector<FunctionInput> testSet = layers.layers[2];
	testSet.pop_back();

	printAllSymmetryGroupsForInputSet(testSet);
	printAllSymmetryGroupsForInputSetFast(testSet);*/

	std::vector<std::vector<SymmetryGroup>> result = findAllSymmetryGroupsFast(layers.layers[3]);
	for(size_t s = 0; s < result.size(); s++) {
		std::cout << "Size " << s << ": " << result[s].size() << " different groups" << std::endl;
	}
	std::cout << "Done!" << std::endl;

	/*ZLayerStack layers7 = generateLayers(7);
	std::cout << layers7 << std::endl;
	
	printAllSymmetryGroupsForInputSet(layers7.layers[2]);*/

	//std::cout << "5: " << layers.layers[5] << std::endl;
	//std::cout << computeSymmetryGroupValues(layers, 5) << std::endl;
	/*std::cout << "4: " << layers.layers[4] << std::endl;
	std::cout << computeSymmetryGroupValues(layers, 4) << std::endl;
	std::cout << "3: " << layers.layers[3] << std::endl;
	std::cout << computeSymmetryGroupValues(layers, 3) << std::endl;
	std::cout << "2: " << layers.layers[2] << std::endl;
	std::cout << computeSymmetryGroupValues(layers, 2) << std::endl;
	std::cout << "1: " << layers.layers[1] << std::endl;
	std::cout << computeSymmetryGroupValues(layers, 1) << std::endl;
	std::cout << "0: " << layers.layers[0] << std::endl;
	std::cout << computeSymmetryGroupValues(layers, 0) << std::endl;*/
	

	/*std::cout << "Subgroups of " << layers.layers[3] << " of size 10" << std::endl;
	auto start = std::chrono::high_resolution_clock::now();
	printSymmetryGroupsForInputSetAndGroupSize(layers.layers[3], 10);
	auto delta = std::chrono::high_resolution_clock::now() - start;
	std::cout << "Took " << delta.count() * 1.0E-9 << " seconds!";*/

	//printAllSymmetryGroupsForInputSet(layers.layers[2]);
}
