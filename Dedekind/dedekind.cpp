
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
#include "valuedDecomposition.h"
#include "layerStack.h"
#include "toString.h"

#include "timeTracker.h"
#include "codeGen.h"

#include "parallelIter.h"

#include <mutex>
#include <atomic>



#define DEBUG_PRINT(v) std::cout << v

template<typename ExtraInfo, typename Func>
void forEachSubsetOfInputSet(const FunctionInputSet& availableOptions, const LayerDecomposition<ExtraInfo>& eqClasses, Func func) {
	for(size_t offCount = 0; offCount <= availableOptions.size(); offCount++) {
		forEachSubgroup(availableOptions, offCount, [&eqClassesOffCnt = eqClasses[offCount], &func](const FunctionInputSet& subGroup) {
			PreprocessedFunctionInputSet preprocessed = preprocess(subGroup);
			func(subGroup, eqClassesOffCnt.get(preprocessed).value, 1);
		});
	}
}

bigInt computeValueOfClass(const FunctionInputSet& availableOptions, const LayerDecomposition<ValueCounted>& decomposition) {
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

valueInt getTotalValueForLayer(const LayerDecomposition<ValueCounted>& eqClassesOfLayer) {
	valueInt totalValueForFullLayer = 0;

	for(const BakedEquivalenceClassMap<EquivalenceClassInfo<ValueCounted>>& eqMap : eqClassesOfLayer) {
		for(const BakedEquivalenceClass<EquivalenceClassInfo<ValueCounted>>& classOfPrevLayer : eqMap) {
			totalValueForFullLayer += classOfPrevLayer.count * classOfPrevLayer.value;
		}
	}

	return totalValueForFullLayer;
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
	int dedekindOrder = 4;
	DedekindDecomposition<ValueCounted> fullDecomposition(dedekindOrder);
	assignValues(fullDecomposition);
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

	/*FunctionInputSet fi{FunctionInput{0b1}, FunctionInput{0b10},FunctionInput{0b100},FunctionInput{0b1000}};
	std::cout << fi << "\n";
	PreprocessedFunctionInputSet pfi = preprocess(fi);
	std::cout << pfi << "\n";
	EquivalenceClass eqfi(pfi);
	std::cout << eqfi.functionInputSet << "\n";
	FunctionInputSet outFi = eqfi.asFunctionInputSet();
	std::cout << outFi << "\n";*/

	
	std::cout << "Decomposition:\n" << fullDecomposition << "\n";
	//std::cout << "Dedekind " << dedekindOrder << " = " << fullDecomposition.bottom().value + 1 << std::endl;

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
