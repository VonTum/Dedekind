#include "layerDecomposition.h"

#include "parallelIter.h"

#include <iostream>
#include <shared_mutex>

static void createLinkBetween(KeyValue<EquivalenceClass, TempEquivClassInfo>& cl, KeyValue<EquivalenceClass, TempEquivClassInfo>& extended, countInt count) {
	cl.value.extendedClasses.push_back(TempEquivClassInfo::AdjacentClass{&extended, count});
}
static void incrementLinkBetween(KeyValue<EquivalenceClass, TempEquivClassInfo>& cl, KeyValue<EquivalenceClass, TempEquivClassInfo>& extended) {
	for(auto& existingElem : cl.value.extendedClasses) {
		if(existingElem.node == &extended) {
			existingElem.formationCount++;
			return;
		}
	}
	createLinkBetween(cl, extended, 1);
}

static const size_t layer7Sizes[]{
	1,1,3,10,38,137,509,1760,5557,15709,39433,87659,172933,303277,473827,660950,824410,920446,920446,824410,660950,473827,303277,172933,87659,39433,15709,5557,1760,509,137,38,10,3,1,1
};

std::vector<EquivalenceClassMap<TempEquivClassInfo>> createDecomposition(const FullLayer& layer) {
	std::vector<EquivalenceClassMap<TempEquivClassInfo>> equivalenceClasses(layer.size() + 1);

	equivalenceClasses[0].resizeWithClear(1);
	equivalenceClasses[1].resizeWithClear(1);
	equivalenceClasses[layer.size()].resizeWithClear(1);

	KeyValue<EquivalenceClass, TempEquivClassInfo>& layer0item = equivalenceClasses[0].getOrAdd(EquivalenceClass::emptyEquivalenceClass, TempEquivClassInfo{}); // equivalence classes of size 0, only one
	KeyValue<EquivalenceClass, TempEquivClassInfo>& layer1item = equivalenceClasses[1].getOrAdd(preprocess(FunctionInputSet{layer[0]}), TempEquivClassInfo{}); // equivalence classes of size 1, only one
	createLinkBetween(layer0item, layer1item, layer.size());

	for(size_t groupSize = 2; groupSize < layer.size(); groupSize++) {
		std::cout << "Looking at size " << groupSize << '/' << layer.size();
		EquivalenceClassMap<TempEquivClassInfo>& curGroups = equivalenceClasses[groupSize];
		if(layer.size() == 35) { // One of the big middle layers
			curGroups.resizeWithClear(layer7Sizes[groupSize]);
		} else {
			curGroups.resizeWithClear(1044); // could be more fine-grained, but this is fine
		}
		std::mutex modifyLock;
		// try to extend each of the previous groups by 1
		iterCollectionInParallel(equivalenceClasses[groupSize - 1], [&layer,&curGroups,&modifyLock,&equivalenceClasses,&groupSize](KeyValue<EquivalenceClass, TempEquivClassInfo>& element) {
			for(FunctionInput newInput : layer) {
				if(element.key.hasFunctionInput(newInput)) continue; // only try to add new inputs that are not yet part of this
				PreprocessedFunctionInputSet resultingPreprocessed = element.key.extendedBy(newInput);
				uint64_t hash = resultingPreprocessed.hash();

				{// this whole bit is to reduce contention on the modifyMutex of the curGroups map, by finding existing classes earlier and handling them without having to take a write lock
					KeyValue<EquivalenceClass, TempEquivClassInfo>* existingClass = curGroups.find(resultingPreprocessed, hash);

					if(existingClass != nullptr) {
						incrementLinkBetween(element, *existingClass);
						continue;
					}
				}

				modifyLock.lock();
				KeyValue<EquivalenceClass, TempEquivClassInfo>& extended = curGroups.getOrAdd(resultingPreprocessed, TempEquivClassInfo{}, hash);
				modifyLock.unlock();

				incrementLinkBetween(element, extended);
			}
		});
		curGroups.shrink();
#ifndef NDEBUG
		for(const auto& item : equivalenceClasses[groupSize - 1]) {
			assert(item.value.extendedClasses.size() >= 1);
		}
#endif
		std::cout << " done! " << curGroups.size() << " classes found!" << std::endl;
	}
	if(layer.size() > 1) {
		KeyValue<EquivalenceClass, TempEquivClassInfo>& lastLayer = equivalenceClasses[layer.size()].getOrAdd(EquivalenceClass(preprocess(layer)), TempEquivClassInfo{});
		for(KeyValue<EquivalenceClass, TempEquivClassInfo>& oneToLast : equivalenceClasses[layer.size() - 1]) {
			createLinkBetween(oneToLast, lastLayer, 1);
		}
	}

	return equivalenceClasses;
}
