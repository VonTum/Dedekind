#include "layerDecomposition.h"

#include "parallelIter.h"

#include <iostream>
#include <shared_mutex>

struct TempEquivClassInfo {
	struct AdjacentClass {
		ValuedEquivalenceClass<TempEquivClassInfo>* node;
		countInt formationCount;
	};

	countInt count;
	std::vector<AdjacentClass> extendedClasses;
	size_t indexInBaked;
};

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

static std::vector<EquivalenceClassMap<TempEquivClassInfo>> createDecomposition(const FullLayer& layer) {
	std::vector<EquivalenceClassMap<TempEquivClassInfo>> equivalenceClasses(layer.size() + 1);

	ValuedEquivalenceClass<TempEquivClassInfo>& layer0item = equivalenceClasses[0].add(EquivalenceClass::emptyEquivalenceClass, TempEquivClassInfo{1}); // equivalence classes of size 0, only one
	ValuedEquivalenceClass<TempEquivClassInfo>& layer1item = equivalenceClasses[1].add(preprocess(FunctionInputSet{layer[0]}), TempEquivClassInfo{layer.size()}); // equivalence classes of size 1, only one
	createLinkBetween(layer0item, layer1item, layer.size());

	for(size_t groupSize = 2; groupSize < layer.size(); groupSize++) {
		std::cout << "Looking at size " << groupSize << '/' << layer.size();
		EquivalenceClassMap<TempEquivClassInfo>& curGroups = equivalenceClasses[groupSize];
		// try to extend each of the previous groups by 1
		std::shared_mutex curGroupsMutex;
		std::mutex editClassMutex;
		iterCollectionInParallel(equivalenceClasses[groupSize - 1], [&layer,&curGroups,&curGroupsMutex,&editClassMutex](ValuedEquivalenceClass<TempEquivClassInfo>& element) {
			countInt countOfCur = element.value.count;
			std::vector<ValuedEquivalenceClass<TempEquivClassInfo>*> foundExistingClasses;
			foundExistingClasses.reserve(3);
			for(FunctionInput newInput : layer) {
				foundExistingClasses.clear();
				if(element.equivClass.hasFunctionInput(newInput)) continue; // only try to add new inputs that are not yet part of this
				PreprocessedFunctionInputSet resultingPreprocessed = element.equivClass.extendedBy(newInput);
				uint64_t hash = resultingPreprocessed.hash();

				{// this whole bit is to reduce contention on curGroupsMutex, by finding existing classes earlier and handling them without having to take a write lock
					curGroupsMutex.lock_shared();
					curGroups.findAllNodesMatchingHash(hash, foundExistingClasses);
					curGroupsMutex.unlock_shared();

					ValuedEquivalenceClass<TempEquivClassInfo>* existingClass = oneOfListContains(foundExistingClasses, resultingPreprocessed);
					if(existingClass != nullptr) {
						editClassMutex.lock();
						existingClass->value.count += countOfCur;
						incrementLinkBetween(element, *existingClass);
						editClassMutex.unlock();
						continue;
					}
				}

				curGroupsMutex.lock();
				ValuedEquivalenceClass<TempEquivClassInfo>& extended = curGroups.getOrAdd(resultingPreprocessed, TempEquivClassInfo{0}, hash);
				curGroupsMutex.unlock();
				
				editClassMutex.lock();
				extended.value.count += countOfCur;
				incrementLinkBetween(element, extended);
				editClassMutex.unlock();
			}
		});
		// all groups covered, add new groups to list, apply correction and repeat
		for(ValuedEquivalenceClass<TempEquivClassInfo>& newGroup : curGroups) {
			assert(newGroup.value.count % groupSize == 0);
			newGroup.value.count /= groupSize;
		}
		std::cout << " done! " << curGroups.size() << " classes found!" << std::endl;
	}
	if(layer.size() > 1) {
		ValuedEquivalenceClass<TempEquivClassInfo>& lastLayer = equivalenceClasses[layer.size()].add(EquivalenceClass(preprocess(layer)), TempEquivClassInfo{1});
		for(ValuedEquivalenceClass<TempEquivClassInfo>& oneToLast : equivalenceClasses[layer.size() - 1]) {
			createLinkBetween(oneToLast, lastLayer, 1);
		}
	}

	return equivalenceClasses;
}

LayerDecomposition::LayerDecomposition(const FullLayer& layer) : equivalenceClasses(layer.size() + 1) {
	std::cout << "Creating Layer decomposition for layer size " << layer.size() << "\n";

	std::vector<EquivalenceClassMap<TempEquivClassInfo>> existingDecomp = createDecomposition(layer);

	std::cout << "Baking: copying over\n";

	// convert to Baked map
	size_t totalConnectionCount = 0; // used for finding the size of the final array which will contain the connections between EquivalenceClasses
	for(size_t i = 0; i < layer.size() + 1; i++) {
		EquivalenceClassMap<TempEquivClassInfo>& extracting = existingDecomp[i];
		BakedEquivalenceClassMap<EquivalenceClassInfo>& newMap = this->equivalenceClasses[i];
		new(&newMap) BakedEquivalenceClassMap<EquivalenceClassInfo>(extracting.size(), extracting.buckets);

		size_t curIndex = 0;
		for(size_t curBucket = 0; curBucket < extracting.buckets; curBucket++) {
			newMap.buckets[curBucket] = newMap.allClasses.ptr() + curIndex;
			for(EquivalenceClassMap<TempEquivClassInfo>::MapNode* cur = extracting.hashTable[curBucket]; cur != nullptr; cur = cur->nextNode) {
				TempEquivClassInfo& oldInfo = cur->item.value;
				new(&newMap[curIndex].eqClass) EquivalenceClass(std::move(cur->item.equivClass));
				newMap[curIndex].value.count = oldInfo.count;
				curIndex++;
				totalConnectionCount += oldInfo.extendedClasses.size();
				oldInfo.indexInBaked = curIndex;
			}
		}
	}
	
	std::cout << "Baking: constructing extended lists\n";

	EquivalenceClassInfo::NextClass* nextClassBuf = new EquivalenceClassInfo::NextClass[totalConnectionCount];
	EquivalenceClassInfo::NextClass* curBufIndex = nextClassBuf;
	for(size_t i = 0; i < layer.size() + 1; i++) {
		EquivalenceClassMap<TempEquivClassInfo>& extracting = existingDecomp[i];
		BakedEquivalenceClassMap<EquivalenceClassInfo>& newMap = this->equivalenceClasses[i];

		size_t curIndex = 0;
		for(size_t curBucket = 0; curBucket < extracting.buckets; curBucket++) {
			for(EquivalenceClassMap<TempEquivClassInfo>::MapNode* cur = extracting.hashTable[curBucket]; cur != nullptr; cur = cur->nextNode) {
				TempEquivClassInfo& oldInfo = cur->item.value;
				EquivalenceClassInfo& info = newMap[curIndex].value;
				info.nextClasses = curBufIndex;
				info.numberOfNextClasses = oldInfo.extendedClasses.size();
				curBufIndex += oldInfo.extendedClasses.size();
				for(size_t j = 0; j < oldInfo.extendedClasses.size(); j++) {
					EquivalenceClassInfo::NextClass newItem(oldInfo.extendedClasses[j].node->value.indexInBaked, oldInfo.extendedClasses[j].formationCount);
					info.nextClasses[j] = newItem;
				}
				curIndex++;
			}
		}
	}
	assert(curBufIndex == nextClassBuf + totalConnectionCount);

	std::cout << "Baking: finding inverses\n";
	for(size_t i = 0; i < layer.size() + 1; i++) {
		EquivalenceMap& inverseMap = this->equivalenceClasses[layer.size() - i];
		for(EquivalenceNode& curClass : this->equivalenceClasses[i]) {
			PreprocessedFunctionInputSet invPrep = preprocess(invert(curClass.eqClass.functionInputSet, layer));
			curClass.value.inverse = inverseMap.indexOf(invPrep);
		}
	}
	for(size_t i = 0; i < layer.size() + 1; i++) {
		EquivalenceMap& curMap = this->equivalenceClasses[i];
		EquivalenceMap& inverseMap = this->equivalenceClasses[layer.size() - i];
		for(int i = 0; i < curMap.size(); i++) {
			assert(inverseMap[curMap[i].value.inverse].value.inverse == i);
		}
	}

	std::cout << "Baking done!\n";

}