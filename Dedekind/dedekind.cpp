
#include <iostream>
#include <cassert>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <algorithm>

#include "functionInput.h"
#include "functionInputSet.h"
#include "equivalenceClass.h"
#include "equivalenceClassMap.h"
#include "layerDecomposition.h"
#include "dedekindDecomposition.h"
#include "layerStack.h"
#include "toString.h"

#include "timeTracker.h"
#include "codeGen.h"

#include "parallelIter.h"

#include <mutex>
#include <atomic>



#define DEBUG_PRINT(v) std::cout << v

std::ostream& operator<<(std::ostream& os, const EquivalenceClassInfo& info) {
	os << "{c=" << info.count << ",v=" << info.value << "}";
	return os;
}

template<typename Func>
void forEachSubsetOfInputSet(const FunctionInputSet& availableOptions, const LayerDecomposition& eqClasses, Func func) {
	for(size_t offCount = 0; offCount <= availableOptions.size(); offCount++) {
		forEachSubgroup(availableOptions, offCount, [&eqClassesOffCnt = eqClasses[offCount], &func](const FunctionInputSet& subGroup) {
			PreprocessedFunctionInputSet preprocessed = preprocess(subGroup);
			func(subGroup, eqClassesOffCnt.get(preprocessed).value.value, 1);
		});
	}
}

template<typename Func>
void forEachSubClassOfInputSet(const FunctionInputSet& availableOptions, const LayerDecomposition& eqClasses, Func func) {
	
}

bigInt computeValueOfClass(const FunctionInputSet& availableOptions, const LayerDecomposition& decomposition) {
	bigInt totalOptions = 0; // 1 for everything on, no further options

	forEachSubsetOfInputSet(availableOptions, decomposition, [&totalOptions](const FunctionInputSet& subGroup, valueInt subGroupValue, countInt occurenceCount) {
		totalOptions += subGroupValue * occurenceCount;
	});

	return totalOptions;
}

template<size_t Size>
FunctionInputSet computeAvailableElements(const std::bitset<Size>& offInputSet, const FullLayer& curLayer, const FullLayer& layerAbove) {
	FunctionInputSet onInputSet = invert(offInputSet, curLayer);
	return removeForcedOn(layerAbove, onInputSet);
}

valueInt getTotalValueForLayer(const LayerDecomposition& eqClassesOfLayer) {
	valueInt totalValueForFullLayer = 0;

	for(const BakedEquivalenceClassMap<EquivalenceClassInfo>& eqMap : eqClassesOfLayer) {
		for(const BakedValuedEquivalenceClass<EquivalenceClassInfo>& classOfPrevLayer : eqMap) {
			const EquivalenceClassInfo& info = classOfPrevLayer.value;
			totalValueForFullLayer += info.count * info.value;
		}
	}

	return totalValueForFullLayer;
}

static LayerDecomposition computeNextLayerValues(const FullLayer& curLayer, const FullLayer& prevLayer, const LayerDecomposition& resultOfPrevLayer) {
	DEBUG_PRINT("Normal Next Layer\n");
	LayerDecomposition decomposition(curLayer);
	// resulting values of empty set, and full set
	decomposition[0].get(PreprocessedFunctionInputSet::emptyPreprocessedFunctionInputSet).value.value = 1;
	decomposition[curLayer.size()].get(preprocess(curLayer)).value.value = getTotalValueForLayer(resultOfPrevLayer);
	
	// for all other group sizes between the empty set and the full set
	for(size_t setSize = 1; setSize < curLayer.size(); setSize++) {
		DEBUG_PRINT("Assigning values of " << setSize << "/" << curLayer.size() << "\n");
		for(BakedValuedEquivalenceClass<EquivalenceClassInfo>& countedClass : decomposition[setSize]) {
			FunctionInputSet availableElementsInPrevLayer = computeAvailableElements(countedClass.eqClass.functionInputSet, curLayer, prevLayer);

			valueInt valueOfSG = computeValueOfClass(availableElementsInPrevLayer, resultOfPrevLayer);

			countedClass.value.value = valueOfSG;
		}
	}
	return decomposition;
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

static LayerDecomposition computeSkipLayerValues(const FullLayer& curLayer, const FullLayer& skippedLayer, const FullLayer& prevLayer, const LayerDecomposition& resultOfPrevLayer) {
	DEBUG_PRINT("SKIP Layer\n");

	LayerDecomposition decomposition(curLayer);
	// resulting values of empty set, and full set
	decomposition[0].get(PreprocessedFunctionInputSet::emptyPreprocessedFunctionInputSet).value.value = 1;
	{
		std::atomic<valueInt> totalValueForFinalElement = 0;
		for(const BakedEquivalenceClassMap<EquivalenceClassInfo>& eqMap : resultOfPrevLayer) {
			iterCollectionInParallel(eqMap, [&totalValueForFinalElement,&skippedLayer](const BakedValuedEquivalenceClass<EquivalenceClassInfo>& countedClass) {
				const EquivalenceClassInfo& info = countedClass.value;
				int numAvailableInSkippedLayer = getNumberOfAvailableInSkippedLayer(skippedLayer, countedClass.eqClass.asFunctionInputSet());
				totalValueForFinalElement += info.count * (info.value << numAvailableInSkippedLayer);
			});
		}
		decomposition[curLayer.size()].get(preprocess(curLayer)).value.value = totalValueForFinalElement;
	}
	// for all other group sizes between the empty set and the full set
	for(size_t setSize = 1; setSize < curLayer.size(); setSize++) {
		DEBUG_PRINT("Assigning values of " << setSize << "/" << curLayer.size() << "\n");
		std::mutex countedClassMutex;
		//iterCollectionInParallel(decomposition[setSize], [&curLayer, &prevLayer, &skippedLayer, &resultOfPrevLayer, &countedClassMutex](BakedValuedEquivalenceClass<EquivalenceClassInfo>& countedClass) {
		for(BakedValuedEquivalenceClass<EquivalenceClassInfo>& countedClass : decomposition[setSize]) {
			std::lock_guard<std::mutex> lg(countedClassMutex);
			FunctionInputSet onInCurLayer = invert(countedClass.eqClass.functionInputSet, curLayer);
			FunctionInputSet availableAbove = removeForcedOn(prevLayer, onInCurLayer);
			FunctionInputSet availableSkipped = removeForcedOn(skippedLayer, onInCurLayer);

			valueInt valueOfSG = 0;

			forEachSubsetOfInputSet(availableAbove, resultOfPrevLayer, [&valueOfSG, &availableSkipped](const FunctionInputSet& chosenOffAbove, valueInt subSetValue, countInt occurenceCount) {
				int numAvailableInSkippedLayer = getNumberOfAvailableInSkippedLayer(availableSkipped, chosenOffAbove);
				valueOfSG += occurenceCount * (subSetValue << numAvailableInSkippedLayer);
			});

			countedClass.value.value = valueOfSG;
		}//);
	}
	return decomposition;
}

static LayerDecomposition getInitialEquivalenceClasses(const FullLayer& firstLayer) {
	assert(firstLayer.size() == 1);
	LayerDecomposition decomposition(firstLayer);
	(*decomposition[0].begin()).value.value = 1;
	(*decomposition[1].begin()).value.value = 1;
	return decomposition;
}

valueInt dedekind(int order) {
	std::cout << "dedekind(" << order << ") = \n";
	LayerStack layers = generateLayers(order);

	LayerDecomposition curValue = getInitialEquivalenceClasses(layers.layers.back());

	DedekindDecomposition fullDecomposition(order);


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

	LayerDecomposition curValue = getInitialEquivalenceClasses(layers.layers[7]);
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

	LayerDecomposition curValue = getInitialEquivalenceClasses(layers.layers[6]);
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

	LayerDecomposition curValue = getInitialEquivalenceClasses(layers.layers[4]);
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

#include <Windows.h>

int main() {
	TimeTracker timer;
	//__debugbreak();
	DedekindDecomposition fullDecomposition(7);
	//Sleep(1000);
	//__debugbreak();
	//return 0;

	/*dedekind(1);
	dedekind(2);
	dedekind(3);
	dedekind(4);
	dedekind(5);*/
	//dedekind(6);
	//dedekind6();
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

void printAllSymmetryGroupsForInputSetFast(const FullLayer& inputSet) {
	/*LayerDecomposition allSets(inputSet);
	std::cout << "Symmetry groups for: " << std::endl << inputSet << std::endl;
	for(size_t size = 0; size <= inputSet.size(); size++) {
		std::cout << "Size " << size << " => " << allSets[size].size() << " groups" << std::endl << allSets[size] << std::endl;
	}*/
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
