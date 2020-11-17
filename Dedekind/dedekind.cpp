
#include <iostream>
#include <cassert>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <algorithm>

#include "functionInput.h"
#include "functionInputSet.h"
#include "equivalenceClass.h"
#include "layerStack.h"
#include "toString.h"

#include "timeTracker.h"
#include "codeGen.h"


typedef int64_t bigInt;
typedef int64_t countInt;
typedef int64_t valueInt;

#define DEBUG_PRINT(v) std::cout << v

struct EquivClassInfo {
	countInt count;
	valueInt value;

	EquivClassInfo() = default;
	EquivClassInfo(countInt count) : count(count), value(0) {}
	EquivClassInfo(countInt count, valueInt value) : count(count), value(value) {}
};

std::ostream& operator<<(std::ostream& os, const EquivClassInfo& info) {
	os << "{c=" << info.count << ",v=" << info.value << "}";
	return os;
}

std::vector<EquivalenceClassMap<EquivClassInfo>> findAllEquivalenceClasses(const FunctionInputSet& available) {
	std::vector<EquivalenceClassMap<EquivClassInfo>> foundGroups(available.size() + 1);

	foundGroups[0].add(EquivalenceClass::emptyEquivalenceClass, 1); // equivalence classes of size 0, only one
	foundGroups[1].add(preprocess(FunctionInputSet{available[0]}), available.size()); // equivalence classes of size 1, only one

	for(size_t groupSize = 2; groupSize < available.size(); groupSize++) {
		DEBUG_PRINT("Looking at size " << groupSize << '/' << available.size());
		const EquivalenceClassMap<EquivClassInfo>& prevGroups = foundGroups[groupSize - 1];
		EquivalenceClassMap<EquivClassInfo>& curGroups = foundGroups[groupSize];
		// try to extend each of the previous groups by 1
		for(const std::pair<EquivalenceClass, EquivClassInfo>& element : prevGroups) {
			const EquivalenceClass& currentlyExtending = element.first;
			countInt countOfCur = element.second.count;
			for(FunctionInput newInput : available) {
				if(currentlyExtending.hasFunctionInput(newInput)) continue; // only try to add new inputs that are not yet part of this
				EquivClassInfo& countOfFoundGroup = curGroups.getOrDefault(element.first.extendedBy(newInput), 0);
				countOfFoundGroup.count += countOfCur;
			}
		}
		// all groups covered, add new groups to list, apply correction and repeat
		for(std::pair<EquivalenceClass, EquivClassInfo>& newGroup : curGroups) {
			assert(newGroup.second.count % groupSize == 0);
			newGroup.second.count /= groupSize;
		}
		DEBUG_PRINT(" done! " << curGroups.size() << " groups found!" << std::endl);
	}
	if(available.size() > 1) foundGroups[available.size()].add(EquivalenceClass(preprocess(available)), 1);

	return foundGroups;
}

template<typename T, typename Func>
void forEachSubsetOfInputSet(const FunctionInputSet& availableOptions, const std::vector<EquivalenceClassMap<T>>& eqClasses, Func func) {
	for(size_t offCount = 0; offCount <= availableOptions.size(); offCount++) {
		const EquivalenceClassMap<T>& relevantSymmetryGroupsOfLayerAbove = eqClasses[offCount];
		forEachSubgroup(availableOptions, offCount, [&eqClassesOffCnt = eqClasses[offCount], &func](const FunctionInputSet& subGroup) {
			PreprocessedFunctionInputSet preprocessed = preprocess(subGroup);
			func(subGroup, eqClassesOffCnt.get(preprocessed).value);
		});
	}
}

bigInt computeValueOfClass(const FunctionInputSet& availableOptions, const std::vector<EquivalenceClassMap<EquivClassInfo>>& eqClasses) {
	bigInt totalOptions = 1; // 1 for everything on, no further options

	for(size_t offCount = 1; offCount <= availableOptions.size(); offCount++) {
		const EquivalenceClassMap<EquivClassInfo>& relevantSymmetryGroupsOfLayerAbove = eqClasses[offCount];
		forEachSubgroup(availableOptions, offCount, [&eqClassesOffCnt = eqClasses[offCount], &totalOptions](const FunctionInputSet& subGroup) {
			PreprocessedFunctionInputSet preprocessed = preprocess(subGroup);
			totalOptions += eqClassesOffCnt.get(preprocessed).value;
		});
	}

	return totalOptions;
}

FunctionInputSet computeAvailableElements(const FunctionInputSet& offInputSet, const FullLayer& curLayer, const FullLayer& layerAbove) {
	FunctionInputSet onInputSet = invert(offInputSet, curLayer);
	return removeForcedOn(layerAbove, onInputSet);
}

valueInt getTotalValueForLayer(const std::vector<EquivalenceClassMap<EquivClassInfo>>& eqClassesOfLayer) {
	valueInt totalValueForFullLayer = 0;

	for(const EquivalenceClassMap<EquivClassInfo>& eqMap : eqClassesOfLayer) {
		for(const std::pair<EquivalenceClass, EquivClassInfo>& classOfPrevLayer : eqMap) {
			const EquivClassInfo& info = classOfPrevLayer.second;
			totalValueForFullLayer += info.count * info.value;
		}
	}

	return totalValueForFullLayer;
}

static std::vector<EquivalenceClassMap<EquivClassInfo>> computeNextLayerValues(const FullLayer& curLayer, const FullLayer& prevLayer, const std::vector<EquivalenceClassMap<EquivClassInfo>>& resultOfPrevLayer) {
	DEBUG_PRINT("Normal Next Layer\n");
	std::vector<EquivalenceClassMap<EquivClassInfo>> resultingValues = findAllEquivalenceClasses(curLayer);
	// resulting values of empty set, and full set
	resultingValues[0].get(EquivalenceClass::emptyEquivalenceClass).value = 1;
	resultingValues[curLayer.size()].get(preprocess(curLayer)).value = getTotalValueForLayer(resultOfPrevLayer);
	
	// for all other group sizes between the empty set and the full set
	for(size_t setSize = 1; setSize < curLayer.size(); setSize++) {
		DEBUG_PRINT("Assigning values of " << setSize << "/" << curLayer.size() << "\n");
		for(std::pair<EquivalenceClass, EquivClassInfo>& countedClass : resultingValues[setSize]) {
			FunctionInputSet availableElementsInPrevLayer = computeAvailableElements(countedClass.first.functionInputSet, curLayer, prevLayer);

			valueInt valueOfSG = computeValueOfClass(availableElementsInPrevLayer, resultOfPrevLayer);

			countedClass.second.value = valueOfSG;
		}
	}
	return resultingValues;
}

static int getNumberOfAvailableInSkippedLayer(const FunctionInputSet& availableElementsInSkippedLayer, const FunctionInputSet& offAbove) {
	int total = 0;
	for(FunctionInput fi : availableElementsInSkippedLayer) {
		if(!isForcedOffBy(fi, offAbove)) {
			total++;
		}
	}
	return total;
}

static std::vector<EquivalenceClassMap<EquivClassInfo>> computeSkipLayerValues(const FullLayer& curLayer, const FullLayer& skippedLayer, const FullLayer& prevLayer, const std::vector<EquivalenceClassMap<EquivClassInfo>>& resultOfPrevLayer) {
	DEBUG_PRINT("SKIP Layer\n");

	std::vector<EquivalenceClassMap<EquivClassInfo>> resultingValues = findAllEquivalenceClasses(curLayer);
	// resulting values of empty set, and full set
	resultingValues[0].get(PreprocessedFunctionInputSet::emptyPreprocessedFunctionInputSet).value = 1;
	{
		valueInt totalValueForFinalElement = 0;
		for(const EquivalenceClassMap<EquivClassInfo>& eqMap : resultOfPrevLayer) {
			for(const std::pair<EquivalenceClass, EquivClassInfo>& countedClass : eqMap) {
				const EquivClassInfo& info = countedClass.second;
				int numAvailableInSkippedLayer = getNumberOfAvailableInSkippedLayer(skippedLayer, countedClass.first.functionInputSet);
				totalValueForFinalElement += info.count * (info.value << numAvailableInSkippedLayer);
			}
		}
		resultingValues[curLayer.size()].get(preprocess(curLayer)).value = totalValueForFinalElement;
	}
	// for all other group sizes between the empty set and the full set
	for(size_t setSize = 1; setSize < curLayer.size(); setSize++) {
		DEBUG_PRINT("Assigning values of " << setSize << "/" << curLayer.size() << "\n");
		for(std::pair<EquivalenceClass, EquivClassInfo>& countedClass : resultingValues[setSize]) {
			FunctionInputSet onInCurLayer = invert(countedClass.first.functionInputSet, curLayer);
			FunctionInputSet availableAbove = removeForcedOn(prevLayer, onInCurLayer);
			FunctionInputSet availableSkipped = removeForcedOn(skippedLayer, onInCurLayer);

			valueInt valueOfSG = 0;

			forEachSubsetOfInputSet(availableAbove, resultOfPrevLayer, [&valueOfSG, &availableSkipped](const FunctionInputSet& chosenOffAbove, valueInt subSetValue) {
				int numAvailableInSkippedLayer = getNumberOfAvailableInSkippedLayer(availableSkipped, chosenOffAbove);
				valueOfSG += subSetValue << numAvailableInSkippedLayer;
			});

			countedClass.second.value = valueOfSG;
		}
	}
	return resultingValues;
}

static std::vector<EquivalenceClassMap<EquivClassInfo>> getInitialSymmetryGroupValues(const FunctionInputSet& firstLayer) {
	assert(firstLayer.size() == 1);
	std::vector<EquivalenceClassMap<EquivClassInfo>> symmetryGroupsOfThisLayer(2);
	symmetryGroupsOfThisLayer[0].add(EquivalenceClass::emptyEquivalenceClass, EquivClassInfo(1, 1));
	symmetryGroupsOfThisLayer[1].add(preprocess(firstLayer), EquivClassInfo(1, 1));
	return symmetryGroupsOfThisLayer;
}

valueInt dedekind(int order) {
	std::cout << "dedekind(" << order << ") = \n";
	LayerStack layers = generateLayers(order);

	std::vector<EquivalenceClassMap<EquivClassInfo>> curValue = getInitialSymmetryGroupValues(layers.layers.back());
	for(size_t i = layers.layers.size()-1; i > 0; i--) {
		curValue = computeNextLayerValues(layers.layers[i-1], layers.layers[i], curValue);
	}

	valueInt result = getTotalValueForLayer(curValue);

	std::cout << result << std::endl;

	return result;
}

valueInt dedekind7() {
	std::cout << "dedicated dedekind(7) = \n";
	LayerStack layers = generateLayers(7);

	std::vector<EquivalenceClassMap<EquivClassInfo>> curValue = getInitialSymmetryGroupValues(layers.layers[7]);
	curValue = computeSkipLayerValues(layers.layers[5], layers.layers[6], layers.layers[7], curValue);
	curValue = computeSkipLayerValues(layers.layers[3], layers.layers[4], layers.layers[5], curValue);
	curValue = computeSkipLayerValues(layers.layers[1], layers.layers[2], layers.layers[3], curValue);
	curValue = computeNextLayerValues(layers.layers[0], layers.layers[1], curValue);

	valueInt result = getTotalValueForLayer(curValue);

	std::cout << result << std::endl;

	return result;
}

valueInt dedekind6() {
	std::cout << "dedicated dedekind(6) = \n";
	LayerStack layers = generateLayers(6);

	std::vector<EquivalenceClassMap<EquivClassInfo>> curValue = getInitialSymmetryGroupValues(layers.layers[6]);
	curValue = computeSkipLayerValues(layers.layers[4], layers.layers[5], layers.layers[6], curValue);
	curValue = computeSkipLayerValues(layers.layers[2], layers.layers[3], layers.layers[4], curValue);
	curValue = computeSkipLayerValues(layers.layers[0], layers.layers[1], layers.layers[2], curValue);

	valueInt result = getTotalValueForLayer(curValue);

	std::cout << result << std::endl;

	return result;
}

valueInt dedekind4() {
	std::cout << "dedicated dedekind(4) = \n";
	LayerStack layers = generateLayers(4);

	std::vector<EquivalenceClassMap<EquivClassInfo>> curValue = getInitialSymmetryGroupValues(layers.layers[4]);
	curValue = computeNextLayerValues(layers.layers[3], layers.layers[4], curValue);
	curValue = computeSkipLayerValues(layers.layers[1], layers.layers[2], layers.layers[3], curValue);
	//curValue = computeNextLayerValues(layers.layers[2], layers.layers[3], curValue);
	//curValue = computeNextLayerValues(layers.layers[1], layers.layers[2], curValue);
	curValue = computeNextLayerValues(layers.layers[0], layers.layers[1], curValue);

	valueInt result = getTotalValueForLayer(curValue);

	std::cout << result << std::endl;

	return result;
}


/*
Correct numbers
	0: 2
	1: 3
	2: 6
	3: 20
	4: 168
	5: 7581
	6: 7828354
	7: 2414682040998
	8: 56130437228687557907788
	9: ????
*/

int main() {
	TimeTracker timer;
	/*dedekind(1);
	dedekind(2);
	dedekind(3);
	dedekind(4);
	dedekind(5);*/
	//dedekind(6);
	dedekind6();
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

	std::vector<std::vector<SymmetryGroup>> result = findAllEquivalenceClasses(layers.layers[3]);
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

void printAllSymmetryGroupsForInputSetFast(const FunctionInputSet& inputSet) {
	auto allSets = findAllEquivalenceClasses(inputSet);
	std::cout << "Symmetry groups for: " << std::endl << inputSet << std::endl;
	for(size_t size = 0; size <= inputSet.size(); size++) {
		std::cout << "Size " << size << " => " << allSets[size].size() << " groups" << std::endl << allSets[size] << std::endl;
	}
}

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

	findAllEquivalenceClasses(layers.layers[2]);

	//printAllSymmetryGroupsForInputSetFast(layers.layers[3]);
}
