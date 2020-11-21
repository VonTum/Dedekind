#include "layerDecomposition.h"

#include <iostream>


static void createLinkBetween(ValuedEquivalenceClass<EquivalenceClassInfo>& subItem, ValuedEquivalenceClass<EquivalenceClassInfo>& extendedItem, countInt count) {
	subItem.value.superClasses.push_back(EquivalenceClassInfo::AdjacentClass{&extendedItem, count});
	extendedItem.value.subClasses.push_back(EquivalenceClassInfo::AdjacentClass{&subItem, count});
}
static void incrementLinkBetween(ValuedEquivalenceClass<EquivalenceClassInfo>& subItem, ValuedEquivalenceClass<EquivalenceClassInfo>& extendedItem) {
	for(auto& subElem : extendedItem.value.subClasses) {
		if(subElem.node == &subItem) {
			for(auto& superElem : subItem.value.superClasses) {
				if(superElem.node == &extendedItem) {
					subElem.formationCount++;
					superElem.formationCount++;
					return;
				}
			}
			assert(false); // Cannot happen, either both must be present or neither!
		}
	}
	createLinkBetween(subItem, extendedItem, 1);
}

LayerDecomposition::LayerDecomposition(const FullLayer& layer) : equivalenceClasses(layer.size() + 1) {

	ValuedEquivalenceClass<EquivalenceClassInfo>& layer0item = equivalenceClasses[0].add(EquivalenceClass::emptyEquivalenceClass, EquivalenceClassInfo{1}); // equivalence classes of size 0, only one
	ValuedEquivalenceClass<EquivalenceClassInfo>& layer1item = equivalenceClasses[1].add(preprocess(FunctionInputSet{layer[0]}), EquivalenceClassInfo{layer.size()}); // equivalence classes of size 1, only one
	createLinkBetween(layer0item, layer1item, layer.size());

	for(size_t groupSize = 2; groupSize < layer.size(); groupSize++) {
		std::cout << "Looking at size " << groupSize << '/' << layer.size();
		EquivalenceClassMap<EquivalenceClassInfo>& curGroups = equivalenceClasses[groupSize];
		// try to extend each of the previous groups by 1
		for(ValuedEquivalenceClass<EquivalenceClassInfo>& element : equivalenceClasses[groupSize - 1]) {
			countInt countOfCur = element.value.count;
			for(FunctionInput newInput : layer) {
				if(element.equivClass.hasFunctionInput(newInput)) continue; // only try to add new inputs that are not yet part of this
				ValuedEquivalenceClass<EquivalenceClassInfo>& extended = curGroups.getOrAdd(element.equivClass.extendedBy(newInput), EquivalenceClassInfo{0});
				extended.value.count += countOfCur;
				incrementLinkBetween(element, extended);
			}
		}
		// all groups covered, add new groups to list, apply correction and repeat
		for(ValuedEquivalenceClass<EquivalenceClassInfo>& newGroup : curGroups) {
			assert(newGroup.value.count % groupSize == 0);
			newGroup.value.count /= groupSize;
		}
		std::cout << " done! " << curGroups.size() << " groups found!" << std::endl;
	}
	if(layer.size() > 1) {
		ValuedEquivalenceClass<EquivalenceClassInfo>& lastLayer = equivalenceClasses[layer.size()].add(EquivalenceClass(preprocess(layer)), EquivalenceClassInfo{1});
		for(ValuedEquivalenceClass<EquivalenceClassInfo>& oneToLast : equivalenceClasses[layer.size() - 1]) {
			createLinkBetween(oneToLast, lastLayer, 1);
		}
	}
}
