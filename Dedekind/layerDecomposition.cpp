#include "layerDecomposition.h"

#include "parallelIter.h"

#include <iostream>
#include <shared_mutex>

static void createLinkBetween(ValuedEquivalenceClass<TempEquivClassInfo>& cl, ValuedEquivalenceClass<TempEquivClassInfo>& extended, countInt count) {
	cl.value.extendedClasses.push_back(TempEquivClassInfo::AdjacentClass{&extended, count});
}
static void incrementLinkBetween(ValuedEquivalenceClass<TempEquivClassInfo>& cl, ValuedEquivalenceClass<TempEquivClassInfo>& extended) {
	for(auto& existingElem : cl.value.extendedClasses) {
		if(existingElem.node == &extended) {
			existingElem.formationCount++;
			return;
		}
	}
	createLinkBetween(cl, extended, 1);
}

static ValuedEquivalenceClass<TempEquivClassInfo>* oneOfListContains(const std::vector<ValuedEquivalenceClass<TempEquivClassInfo>*>& list, const PreprocessedFunctionInputSet& preprocessed) {
	for(ValuedEquivalenceClass<TempEquivClassInfo>* existingClass : list) {
		if(existingClass->equivClass.contains(preprocessed)) {
			return existingClass;
		}
	}
	return nullptr;
}

std::vector<EquivalenceClassMap<TempEquivClassInfo>> createDecomposition(const FullLayer& layer) {
	std::vector<EquivalenceClassMap<TempEquivClassInfo>> equivalenceClasses(layer.size() + 1);

	ValuedEquivalenceClass<TempEquivClassInfo>& layer0item = equivalenceClasses[0].add(EquivalenceClass::emptyEquivalenceClass, TempEquivClassInfo{}); // equivalence classes of size 0, only one
	ValuedEquivalenceClass<TempEquivClassInfo>& layer1item = equivalenceClasses[1].add(preprocess(FunctionInputSet{layer[0]}), TempEquivClassInfo{}); // equivalence classes of size 1, only one
	createLinkBetween(layer0item, layer1item, layer.size());

	for(size_t groupSize = 2; groupSize < layer.size(); groupSize++) {
		std::cout << "Looking at size " << groupSize << '/' << layer.size();
		EquivalenceClassMap<TempEquivClassInfo>& curGroups = equivalenceClasses[groupSize];
		// try to extend each of the previous groups by 1
		std::shared_mutex curGroupsMutex;
		iterCollectionInParallel(equivalenceClasses[groupSize - 1], [&layer,&curGroups,&curGroupsMutex,&equivalenceClasses,&groupSize](ValuedEquivalenceClass<TempEquivClassInfo>& element) {
			for(FunctionInput newInput : layer) {
				if(element.equivClass.hasFunctionInput(newInput)) continue; // only try to add new inputs that are not yet part of this
				PreprocessedFunctionInputSet resultingPreprocessed = element.equivClass.extendedBy(newInput);
				uint64_t hash = resultingPreprocessed.hash();

				{// this whole bit is to reduce contention on curGroupsMutex, by finding existing classes earlier and handling them without having to take a write lock
					curGroupsMutex.lock_shared();
					ValuedEquivalenceClass<TempEquivClassInfo>* existingClass = curGroups.find(resultingPreprocessed, hash);
					curGroupsMutex.unlock_shared();

					if(existingClass != nullptr) {
						incrementLinkBetween(element, *existingClass);
						continue;
					}
				}

				curGroupsMutex.lock();
				ValuedEquivalenceClass<TempEquivClassInfo>& extended = curGroups.getOrAdd(resultingPreprocessed, TempEquivClassInfo{}, hash);
				curGroupsMutex.unlock();
				
				incrementLinkBetween(element, extended);
			}
		});
#ifndef NDEBUG
		for(const auto& item : equivalenceClasses[groupSize - 1]) {
			assert(item.value.extendedClasses.size() >= 1);
		}
#endif
		std::cout << " done! " << curGroups.size() << " classes found!" << std::endl;
	}
	if(layer.size() > 1) {
		ValuedEquivalenceClass<TempEquivClassInfo>& lastLayer = equivalenceClasses[layer.size()].add(EquivalenceClass(preprocess(layer)), TempEquivClassInfo{});
		for(ValuedEquivalenceClass<TempEquivClassInfo>& oneToLast : equivalenceClasses[layer.size() - 1]) {
			createLinkBetween(oneToLast, lastLayer, 1);
		}
	}

	return equivalenceClasses;
}
