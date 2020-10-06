
#include <iostream>
#include <cassert>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <algorithm>
#include <chrono>

#include "collectionOperations.h"
#include "functionInput.h"


typedef int64_t bigInt;

int64_t factorial(int n) {
	int64_t total = 1;
	for(int i = 2; i <= n; i++) {
		total *= i;
	}
	return total;
}

int choose(int of, int num) {
	assert(of > 0 && num > 0 && num <= of);
	if(num <= of / 2) {
		num = of - num;
	}
	int64_t total = 1;
	for(int i = num + 1; i <= of; i++) {
		total *= i;
	}
	total /= factorial(of - num);
	assert(total <= std::numeric_limits<int>::max());
	return int(total);
}

std::vector<int> computeLayerSizes(int n) {
	n++;
	std::vector<int> layerSizes(n);

	for(int i = 0; i < n; i++) {
		layerSizes[i] = choose(n, i+1);
	}

	return layerSizes;
}

std::vector<FunctionInput> removeSuperInputs(const std::vector<FunctionInput>& layer, const std::vector<FunctionInput>& inputSet) {
	std::vector<FunctionInput> result;
	for(FunctionInput f : layer) {
		for(FunctionInput i : inputSet) {
			if(isSubSet(i, f)) {
				goto skip;
			}
		}
		result.push_back(f);
		skip:;
	}
	return result;
}

struct LayerStack {
	std::vector<std::vector<FunctionInput>> layers;

	/*LayerStack popBottomLayer() const {
		std::vector<std::vector<FunctionInput>> result = layers;
		result.erase(result.begin());
		return LayerStack{result};
	}
	LayerStack withSuperInputsRemoved(const std::vector<FunctionInput>& inputs) const {
		LayerStack result = *this;
		for(std::vector<FunctionInput>& layer : result.layers) {
			layer.erase(std::remove_if(layer.begin(), layer.end(), [inputs](FunctionInput funcToRemove) {
				for(FunctionInput i : inputs) {
					if(isSubSet(i, funcToRemove)) {
						return true;
					}
				}
				return false;
			}), layer.end());
		}
		return result;
	}
	size_t sizeOfBottomLayer() const {
		return layers[0].size();
	}*/
};

std::ostream& operator<<(std::ostream& os, LayerStack layers) {
	os << "LayerStack" << layers.layers;
	return os;
}

LayerStack generateLayers(size_t n) {
	std::vector<std::vector<FunctionInput>> result(n + 1);
	
	int32_t max = 1 << n;

	for(int32_t cur = 0; cur < max; cur++) {
		FunctionInput input{cur};

		int layer = input.getNumberEnabled();

		result[layer].push_back(input);
	}

	return LayerStack{std::move(result)};
}

void printIndent(std::ostream& os, int indent) {
	for(int i = 0; i < indent; i++) {
		os << "  ";
	}
}

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

void assignSymmetryGroups(const std::vector<FunctionInput>& available, size_t groupSize, std::vector<SymmetryGroupValue>& groups) {
	for(SymmetryGroupValue& sgv : groups) sgv.count = 0; // reset all counts
	forEachSubgroup(available, groupSize, [&groups](const std::vector<FunctionInput>& subGroup) {
		PreprocessedFunctionInputSet normalized = preprocess(subGroup);
		for(SymmetryGroupValue& s : groups) {
			if(isIsomorphic(s.example, normalized)) {
				s.count++;
				return; // found group
			}
		}
		// not found => error: group must exist
		throw "error group does not exist";
	});
}

bigInt computeTotalOptionsForAvailableChoices(const std::vector<FunctionInput>& availableOptions, std::vector<std::vector<SymmetryGroupValue>>& symmetryGroups) {
	bigInt totalOptions = 1; // 1 for everything on, no further options

	for(size_t offCount = 1; offCount <= availableOptions.size(); offCount++) {
		std::vector<SymmetryGroupValue>& relevantSymmetryGroupsOfLayerAbove = symmetryGroups[offCount];
		assignSymmetryGroups(availableOptions, offCount, relevantSymmetryGroupsOfLayerAbove);

		for(const SymmetryGroupValue& v : relevantSymmetryGroupsOfLayerAbove) {
			totalOptions += v.count * v.value;
		}
	}

	return totalOptions;
}

std::vector<FunctionInput> computeAvailableElementsInLayerAbove(const std::vector<FunctionInput>& offInputSet, const std::vector<FunctionInput>& curLayer, const std::vector<FunctionInput>& layerAbove) {
	std::vector<FunctionInput> onInputSet = removeAll(curLayer, offInputSet);
	return removeSuperInputs(layerAbove, onInputSet);
}

static HashedNormalizedFunctionInputSet emptyGroup(PreprocessedFunctionInputSet{std::vector<FunctionInput>{}, std::vector<VariableOccurenceGroup>{}, 0}, 0);
std::vector<SymmetryGroupValue> emptyGroupValueList{SymmetryGroupValue(SymmetryGroup{1, emptyGroup}, 1)};

std::vector<SymmetryGroupValue> allContainedGroup(const std::vector<std::vector<SymmetryGroupValue>>& valuesOfLayerAbove, const std::vector<FunctionInput>& thisLayer) {
	SymmetryGroupValue result;

	result.example = hash(preprocess(thisLayer));
	bigInt totalValue = 0;
	for(const std::vector<SymmetryGroupValue>& lst : valuesOfLayerAbove) {
		for(const SymmetryGroupValue& v : lst) {
			totalValue += v.count * v.value;
		}
	}
	result.value = totalValue;
	result.count = 1;

	return std::vector<SymmetryGroupValue>{result};
}

std::vector<std::vector<SymmetryGroupValue>> computeSymmetryGroupValues(const LayerStack& stack, size_t layerIndex) {
	if(layerIndex == stack.layers.size() - 1) {
		return std::vector<std::vector<SymmetryGroupValue>>{emptyGroupValueList, std::vector<SymmetryGroupValue>{SymmetryGroupValue(SymmetryGroup{1, hash(preprocess(stack.layers.back()))}, 1)}};
	}
	std::vector<std::vector<SymmetryGroupValue>> valuesOfLayerAbove = computeSymmetryGroupValues(stack, layerIndex + 1);

	const std::vector<FunctionInput>& curLayer = stack.layers[layerIndex];
	const std::vector<FunctionInput>& layerAbove = stack.layers[layerIndex+1];
	std::vector<std::vector<SymmetryGroupValue>> symmetryGroupsOfThisLayer(curLayer.size()+1);
	symmetryGroupsOfThisLayer[0] = emptyGroupValueList;
	symmetryGroupsOfThisLayer[curLayer.size()] = allContainedGroup(valuesOfLayerAbove, curLayer);
	for(size_t groupSize = 1; groupSize < curLayer.size(); groupSize++) {
		std::vector<SymmetryGroup> symmetryGroupsForGroupSize = findSymmetryGroups(curLayer, groupSize);
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

/*void printSymmetryGroupsForInputSetAndGroupSize(const std::vector<FunctionInput>& inputSet, size_t groupSize) {
	auto sgs = findSymmetryGroups(inputSet, groupSize);
	auto groupsByValue = groupByValue(sgs);
	std::cout << "Size " << groupSize << " => " << sgs.size() << " groups" << std::endl;
}*/

/*void printAllSymmetryGroupsForInputSet(const std::vector<FunctionInput>& inputSet) {
	std::cout << "Symmetry groups for: " << std::endl << inputSet << std::endl;
	for(size_t size = 1; size <= inputSet.size(); size++) {
		printSymmetryGroupsForInputSetAndGroupSize(inputSet, size);
	}
}*/

bigInt dedekind(int order) {
	std::cout << "dedekind(" << order << ") = ";
	LayerStack layers = generateLayers(order);

	auto lastLayerValues = computeSymmetryGroupValues(layers, 0);

	bigInt result = lastLayerValues[1][0].value + 1;

	std::cout << result << std::endl;

	return result;
}

int main() {

	dedekind(1);
	dedekind(2);
	dedekind(3);
	dedekind(4);
	dedekind(5);
	dedekind(6);


	/*forEachSubgroup(std::vector<int>{1, 2, 3, 4, 5}, 3, [](const std::vector<int>& vec) {
		std::cout << vec << std::endl;
	});*/

	/*LayerStack layers6 = generateLayers(6);
	std::cout << layers6 << std::endl;

	printAllSymmetryGroupsForInputSet(layers6.layers[2]);

	LayerStack layers7 = generateLayers(7);
	std::cout << layers7 << std::endl;
	
	printAllSymmetryGroupsForInputSet(layers7.layers[2]);*/

	/*std::cout << "5: " << layers.layers[5] << std::endl;
	std::cout << computeSymmetryGroupValues(layers, 5) << std::endl;
	std::cout << "4: " << layers.layers[4] << std::endl;
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
