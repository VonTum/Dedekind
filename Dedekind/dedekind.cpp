
#include <iostream>
#include <cassert>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <algorithm>
#include <chrono>

#include "functionInput.h"
#include "functionInputSet.h"
#include "equivalenceClass.h"
#include "layerStack.h"
#include "toString.h"

#include "codeGen.h"


typedef int64_t bigInt;
typedef int64_t countInt;
typedef int64_t valueInt;

#define DEBUG_PRINT(v) std::cout << v

std::vector<EquivalenceClassMap<countInt>> findAllSymmetryGroupsFast(const FunctionInputSet& available) {
	std::vector<EquivalenceClassMap<countInt>> foundGroups(available.size() + 1);

	foundGroups[0].add(EquivalenceClass::emptyEquivalenceClass, 1); // equivalence classes of size 0, only one
	foundGroups[1].add(preprocess(FunctionInputSet{available[0]}), countInt(available.size())); // equivalence classes of size 1, only one

	for(size_t groupSize = 2; groupSize < available.size(); groupSize++) {
		DEBUG_PRINT("Looking at size " << groupSize << '/' << available.size());
		const EquivalenceClassMap<countInt>& prevGroups = foundGroups[groupSize - 1];
		EquivalenceClassMap<countInt>& curGroups = foundGroups[groupSize];
		// try to extend each of the previous groups by 1
		for(const std::pair<EquivalenceClass, countInt>& element : prevGroups) {
			const EquivalenceClass& currentlyExtending = element.first;
			countInt countOfCur = element.second;
			for(FunctionInput newInput : available) {
				if(currentlyExtending.hasFunctionInput(newInput)) continue; // only try to add new inputs that are not yet part of this
				countInt& countOfFoundGroup = curGroups.getOrDefault(element.first.extendedBy(newInput), 0);
				countOfFoundGroup += countOfCur;
			}
		}
		// all groups covered, add new groups to list, apply correction and repeat
		for(std::pair<EquivalenceClass, countInt>& newGroup : curGroups) {
			assert(newGroup.second % groupSize == 0);
			newGroup.second /= groupSize;
		}
		DEBUG_PRINT(" done! " << curGroups.size() << " groups found!" << std::endl);
	}
	foundGroups[available.size()].add(EquivalenceClass(preprocess(available)), 1);

	return foundGroups;
}


valueInt countEachSymmetryGroup(const FunctionInputSet& available, size_t groupSize, const EquivalenceClassMap<valueInt>& groups) {
	valueInt result = 0;
	forEachSubgroup(available, groupSize, [&groups, &result](const FunctionInputSet& subGroup) {
		PreprocessedFunctionInputSet preprocessed = preprocess(subGroup);
		result += groups.get(preprocessed);
	});
	return result;
}

bigInt computeTotalOptionsForAvailableChoices(const FunctionInputSet& availableOptions, const std::vector<EquivalenceClassMap<valueInt>>& symmetryGroups) {
	bigInt totalOptions = 1; // 1 for everything on, no further options

	for(size_t offCount = 1; offCount <= availableOptions.size(); offCount++) {
		const EquivalenceClassMap<valueInt>& relevantSymmetryGroupsOfLayerAbove = symmetryGroups[offCount];
		totalOptions += countEachSymmetryGroup(availableOptions, offCount, relevantSymmetryGroupsOfLayerAbove);
	}

	return totalOptions;
}

FunctionInputSet computeAvailableElementsInLayerAbove(const FunctionInputSet& offInputSet, const FunctionInputSet& curLayer, const FunctionInputSet& layerAbove) {
	FunctionInputSet onInputSet = removeAll(curLayer, offInputSet);
	return removeSuperInputs(layerAbove, onInputSet);
}

// returns a group of all Equivalence classes in the layer with their values, and the total value of enabling the entire layer
std::pair<std::vector<EquivalenceClassMap<valueInt>>, valueInt> computeSymmetryGroupValues(const LayerStack& stack, size_t layerIndex) {
	const FunctionInputSet& curLayer = stack.layers[layerIndex];
	std::vector<EquivalenceClassMap<valueInt>> symmetryGroupsOfThisLayer(curLayer.size() + 1);
	symmetryGroupsOfThisLayer[0].add(EquivalenceClass::emptyEquivalenceClass, 1);
	if(layerIndex == stack.layers.size() - 1) {
		symmetryGroupsOfThisLayer[1].add(preprocess(curLayer), 1);
		return std::make_pair(symmetryGroupsOfThisLayer, 2); // 2 possibilities for final layer, final node on, or final node off
	}
	std::pair<std::vector<EquivalenceClassMap<valueInt>>, valueInt> resultOfPrevLayer = computeSymmetryGroupValues(stack, layerIndex + 1);
	const std::vector<EquivalenceClassMap<valueInt>>& valuesOfLayerAbove = resultOfPrevLayer.first;

	std::vector<EquivalenceClassMap<countInt>> symmetryGroupsOfCurLayer = findAllSymmetryGroupsFast(curLayer);
	symmetryGroupsOfThisLayer[curLayer.size()].add(preprocess(curLayer), resultOfPrevLayer.second);

	valueInt totalValueForFullLayer = 1 + resultOfPrevLayer.second; // 1 for empty

	const FunctionInputSet& layerAbove = stack.layers[layerIndex + 1];
	for(size_t groupSize = 1; groupSize < curLayer.size(); groupSize++) {
		DEBUG_PRINT("Assigning values of " << groupSize << "/" << curLayer.size() << "\n");
		const EquivalenceClassMap<countInt>& symmetryGroupsForGroupSize = symmetryGroupsOfCurLayer[groupSize];
		EquivalenceClassMap<valueInt> symmetryGroupValueAssociationsOfThisLayer;
		for(const std::pair<EquivalenceClass, countInt>& countedGroup : symmetryGroupsForGroupSize) {
			FunctionInputSet availableElementsInLayerAbove = computeAvailableElementsInLayerAbove(countedGroup.first.functionInputSet, curLayer, layerAbove);

			bigInt valueOfSG = computeTotalOptionsForAvailableChoices(availableElementsInLayerAbove, valuesOfLayerAbove);

			totalValueForFullLayer += valueOfSG * countedGroup.second;

			symmetryGroupValueAssociationsOfThisLayer.add(countedGroup.first, valueOfSG);
		}
		symmetryGroupsOfThisLayer[groupSize] = symmetryGroupValueAssociationsOfThisLayer;
	}
	return std::make_pair(symmetryGroupsOfThisLayer, totalValueForFullLayer);
}

void printAllSymmetryGroupsForInputSetFast(const FunctionInputSet& inputSet) {
	auto allSets = findAllSymmetryGroupsFast(inputSet);
	std::cout << "Symmetry groups for: " << std::endl << inputSet << std::endl;
	for(size_t size = 0; size <= inputSet.size(); size++) {
		std::cout << "Size " << size << " => " << allSets[size].size() << " groups" << std::endl << allSets[size] << std::endl;
	}
}

valueInt dedekind(int order) {
	std::cout << "dedekind(" << order << ") = \n";
	LayerStack layers = generateLayers(order);

	valueInt result = computeSymmetryGroupValues(layers, 0).second;

	std::cout << result << std::endl;

	return result;
}

class TimeTracker {	
	std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
	std::string message;
public:
	TimeTracker(std::string message) : message(message), startTime(std::chrono::high_resolution_clock::now()) {}
	TimeTracker() : message("Time taken: "), startTime(std::chrono::high_resolution_clock::now()) {}
	~TimeTracker() {
		std::chrono::nanoseconds deltaTime = std::chrono::high_resolution_clock::now() - startTime;
		std::cout << message << deltaTime.count() * 1.0e-6 << "ms";
	}
};

void testPreprocess() {
	FunctionInputSet set;

	set.push_back(FunctionInput{0b000011});
	set.push_back(FunctionInput{0b000110});
	set.push_back(FunctionInput{0b001100});
	set.push_back(FunctionInput{0b011000});
	set.push_back(FunctionInput{0b110000});

	auto prep = preprocess(set);
	std::cout << prep;
}

void testPreprocess2() {
	LayerStack st = generateLayers(6);
	FunctionInputSet inputInputSet = st.layers[2];
	

	for(int i = 1; i < inputInputSet.size(); i++) {
		std::cout << i << "/" << inputInputSet.size() << std::endl;
		forEachSubgroup(inputInputSet, i, [](const FunctionInputSet& funcInputSet) {
			int numberOfVars = span(funcInputSet).getHighestEnabled();
			PreprocessedFunctionInputSet realprep = preprocess(funcInputSet);

			FunctionInputSet permutedSet(funcInputSet.size());
			forEachPermutation(generateIntegers(numberOfVars), [&funcInputSet, &permutedSet, &realprep](const std::vector<int>& permut) {
				swizzleVector(permut, funcInputSet, permutedSet);

				PreprocessedFunctionInputSet prep = preprocess(permutedSet);

				//std::cout << prep << std::endl;

				if(prep.variableOccurences != realprep.variableOccurences) {
					throw "Bad found!";
					__debugbreak();
				}
			});
		});
	}
}

static void testFindAllSymmetryGroupsForInputSet() {
	LayerStack layers = generateLayers(7);
	std::cout << layers << std::endl;

	findAllSymmetryGroupsFast(layers.layers[4]);

	//printAllSymmetryGroupsForInputSetFast(layers.layers[3]);
}

int main() {
	//testFindAllSymmetryGroupsForInputSet(); return 0;

	//testPreprocess2();return 0;
	//genCodeForEquivClass();
	//genCodeForSmallPermut(4);
	//genCodeForAllLargePermut();
	//genCodeForAllPermut();
	//return 0;
	TimeTracker timer;
	/*dedekind(1);
	dedekind(2);
	dedekind(3);
	dedekind(4);
	dedekind(5);*/
	dedekind(7);
	/*dedekind(6);
	dedekind(6);
	dedekind(6);
	dedekind(6);
	dedekind(6);*/
	//dedekind(7);
	
	return 0;//*/

	/*LayerStack layers = generateLayers(4);
	std::cout << layers << std::endl;*/

	std::cout << "Starting" << std::endl;
	LayerStack layers = generateLayers(5);
	std::cout << "Layers made" << std::endl;

	std::cout << layers << std::endl;
	
	FunctionInputSet testSet = layers.layers[2];
	//testSet.pop_back();


	/*printAllSymmetryGroupsForInputSet(testSet);
	printAllSymmetryGroupsForInputSetFast(testSet);

	std::vector<std::vector<SymmetryGroup>> result = findAllSymmetryGroupsFast(layers.layers[3]);
	for(size_t s = 0; s < result.size(); s++) {
		std::cout << "Size " << s << ": " << result[s].size() << " different groups" << std::endl;
	}
	std::cout << "Done!" << std::endl;*/

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
