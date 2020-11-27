#include "layerDecomposition.h"

#include "parallelIter.h"

#include <iostream>

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

static std::vector<EquivalenceClassMap<TempEquivClassInfo>> createDecomposition(const FullLayer& layer) {
	std::vector<EquivalenceClassMap<TempEquivClassInfo>> equivalenceClasses(layer.size() + 1);

	ValuedEquivalenceClass<TempEquivClassInfo>& layer0item = equivalenceClasses[0].add(EquivalenceClass::emptyEquivalenceClass, TempEquivClassInfo{1}); // equivalence classes of size 0, only one
	ValuedEquivalenceClass<TempEquivClassInfo>& layer1item = equivalenceClasses[1].add(preprocess(FunctionInputSet{layer[0]}), TempEquivClassInfo{layer.size()}); // equivalence classes of size 1, only one
	createLinkBetween(layer0item, layer1item, layer.size());

	for(size_t groupSize = 2; groupSize < layer.size(); groupSize++) {
		std::cout << "Looking at size " << groupSize << '/' << layer.size();
		EquivalenceClassMap<TempEquivClassInfo>& curGroups = equivalenceClasses[groupSize];
		// try to extend each of the previous groups by 1
		std::mutex curGroupsMutex;
		iterCollectionInParallel(equivalenceClasses[groupSize - 1], [&layer,&curGroups,&curGroupsMutex](ValuedEquivalenceClass<TempEquivClassInfo>& element) {
			countInt countOfCur = element.value.count;
			for(FunctionInput newInput : layer) {
				if(element.equivClass.hasFunctionInput(newInput)) continue; // only try to add new inputs that are not yet part of this
				PreprocessedFunctionInputSet resultingPreprocessed = element.equivClass.extendedBy(newInput);
				curGroupsMutex.lock();
				ValuedEquivalenceClass<TempEquivClassInfo>& extended = curGroups.getOrAdd(std::move(resultingPreprocessed), TempEquivClassInfo{0});
				extended.value.count += countOfCur;
				incrementLinkBetween(element, extended);
				curGroupsMutex.unlock();
			}
		});
		// all groups covered, add new groups to list, apply correction and repeat
		for(ValuedEquivalenceClass<TempEquivClassInfo>& newGroup : curGroups) {
			assert(newGroup.value.count % groupSize == 0);
			newGroup.value.count /= groupSize;
		}
		std::cout << " done! " << curGroups.size() << " groups found!" << std::endl;
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
		BakedEquivalenceClassMap<EquivalenceClassInfo>& newMap = equivalenceClasses[i];
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

	EquivalenceClassInfo::AdjacentClass* adjacentBuf = new EquivalenceClassInfo::AdjacentClass[totalConnectionCount];
	EquivalenceClassInfo::AdjacentClass* curAdj = adjacentBuf;
	for(size_t i = 0; i < layer.size() + 1; i++) {
		EquivalenceClassMap<TempEquivClassInfo>& extracting = existingDecomp[i];
		BakedEquivalenceClassMap<EquivalenceClassInfo>& newMap = equivalenceClasses[i];

		size_t curIndex = 0;
		for(size_t curBucket = 0; curBucket < extracting.buckets; curBucket++) {
			for(EquivalenceClassMap<TempEquivClassInfo>::MapNode* cur = extracting.hashTable[curBucket]; cur != nullptr; cur = cur->nextNode) {
				TempEquivClassInfo& oldInfo = cur->item.value;
				EquivalenceClassInfo& info = newMap[curIndex].value;
				info.extendedClasses = curAdj;
				info.numberOfExtendedClasses = oldInfo.extendedClasses.size();
				curAdj += oldInfo.extendedClasses.size();
				for(size_t j = 0; j < oldInfo.extendedClasses.size(); j++) {
					info.extendedClasses[j].nodeIndex = oldInfo.extendedClasses[j].node->value.indexInBaked;
					info.extendedClasses[j].formationCount = oldInfo.extendedClasses[j].formationCount;
				}
				curIndex++;
			}
		}
	}

	std::cout << "Baking done!\n";

	assert(curAdj == adjacentBuf + totalConnectionCount);
}
