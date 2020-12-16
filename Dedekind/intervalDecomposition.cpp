#include "intervalDecomposition.h"

#include <map>
#include <shared_mutex>
#include "sharedLockGuard.h"

#include <iostream>
#include "toString.h"

using AvailableChoices = SmallVector<EquivalenceClassIndex, MAX_PREPROCESSED+1>;
using AvailableChoiceFis = SmallVector<FunctionInputSet, MAX_PREPROCESSED+1>;

struct EquivClassIndexCmp {
	bool operator()(const AvailableChoices& a, const AvailableChoices& b) const {
		if(a.size() != b.size()) return a.size() < b.size();

		assert(a.size() == b.size());
		for(size_t i = 0; i < a.size(); i++) {
			if(a[i] != b[i]) {
				return a[i] < b[i];
			}
		}
		return false;
	}
};
using AvailableChoiceMap = std::map<AvailableChoices, valueInt, EquivClassIndexCmp>;

struct MemoizedRecursiveChoices {
	AvailableChoiceMap memoMap;
	mutable std::shared_mutex mut;

	constexpr static int NO_VALUE = -13;

	valueInt get(const AvailableChoices& avc) const {
		shared_lock_guard lg(mut);
		auto iter = memoMap.find(avc);
		if(iter != memoMap.end()) {
			return (*iter).second;
		} else {
			return NO_VALUE;
		}
	}
	void add(AvailableChoices&& avc, valueInt value) {
		std::lock_guard<std::shared_mutex> lg(mut);
		auto iter = memoMap.find(avc);
		if(iter != memoMap.end()) {
			assert((*iter).second == value);
			return;
		} else {
			memoMap.insert(std::make_pair(std::move(avc), value));
		}
	}
};


AvailableChoices produceAvailableChoices(const DedekindDecomposition<IntervalSize>& decomp, const AvailableChoiceFis& availableChoices) {
	AvailableChoices result(availableChoices.size());
	for(size_t i = 0; i < availableChoices.size(); i++) {
		result[i] = decomp[i+1/*because the bottom layer isn't included*/].indexOf(preprocess(availableChoices[i]));
	}
	return result;
}

static valueInt getRecursiveChoices(const DedekindDecomposition<IntervalSize>& decomp, const AvailableChoiceFis& baseNonDominatedLayers, MemoizedRecursiveChoices & memoMap);

static valueInt getRecursiveChoicesNonMemo(const DedekindDecomposition<IntervalSize>& decomp, const AvailableChoiceFis& baseNonDominatedLayers, MemoizedRecursiveChoices & memoMap) {
	if(baseNonDominatedLayers.size() == 0) {
		return 1;
	} else {
		valueInt totalChoices = 0;
		forEachSubgroup(baseNonDominatedLayers.back(), [&decomp, &baseNonDominatedLayers, &totalChoices, &memoMap](const FunctionInputSet& forcingElems) {
			// the forcing elements are elements that force the elements from the layer below. For example, for monotonic boolean functions going false->true, this would be the false-elements. 
			AvailableChoiceFis availableInThisSplit(baseNonDominatedLayers.size() - 1);
			for(size_t i = 0; i < baseNonDominatedLayers.size() - 1; i++) {
				availableInThisSplit[i] = getNonDominatedElements(baseNonDominatedLayers[i], forcingElems);
			}

			totalChoices += getRecursiveChoices(decomp, availableInThisSplit, memoMap);
		});
		return totalChoices;
	}
}

static bool isEntirelyEmpty(const AvailableChoiceFis& baseNonDominatedLayers) {
	for(const FunctionInputSet& fi : baseNonDominatedLayers) {
		if(fi.size() != 0) {
			return false;
		}
	}
	return true;
}

static valueInt getRecursiveChoices(const DedekindDecomposition<IntervalSize>& decomp, const AvailableChoiceFis& baseNonDominatedLayers, MemoizedRecursiveChoices& memoMap) {
	if(isEntirelyEmpty(baseNonDominatedLayers)) {
		return 1;
	} else {
		AvailableChoices avCh = produceAvailableChoices(decomp, baseNonDominatedLayers);
		valueInt memoizedChoices = memoMap.get(avCh);
		if(memoizedChoices == MemoizedRecursiveChoices::NO_VALUE) {
			valueInt totalChoices = getRecursiveChoicesNonMemo(decomp, baseNonDominatedLayers, memoMap);
			memoMap.add(std::move(avCh), totalChoices);
			return totalChoices;
		} else {
			return memoizedChoices;

		}
	}
}

static valueInt computeIntervalSizeToBottom(const DedekindDecomposition<IntervalSize>& decomp, size_t curLayerIndex, const EqClass<IntervalSize>& currentlyFindingIntervalSizeFor, MemoizedRecursiveChoices& memoMap) {
	const LayerDecomposition<IntervalSize>& curLayer = decomp[curLayerIndex];

	FunctionInputSet fis = currentlyFindingIntervalSizeFor.eqClass.asFunctionInputSet();
	AvailableChoiceFis forcedInPrevLayers(curLayerIndex);
	for(size_t i = 1; i < curLayerIndex; i++) {
		forcedInPrevLayers[i-1] = getDominatedElements(decomp.layer(i), fis);
	}
	forcedInPrevLayers[curLayerIndex-1] = std::move(fis);
	return getRecursiveChoices(decomp, forcedInPrevLayers, memoMap);
}

void IntervalSize::populate(DedekindDecomposition<IntervalSize>& decomp) {
	MemoizedRecursiveChoices memoMap;
	decomp[0].full().intervalSizeToBottom = 2;
	for(size_t curLayerIndex = 1; curLayerIndex < decomp.numLayers(); curLayerIndex++) {
		LayerDecomposition<IntervalSize>& curLayer = decomp[curLayerIndex];
		for(size_t setSize = 1; setSize <= curLayer.getNumberOfFunctionInputs(); setSize++) {
			std::cout << "Assigning interval sizes of " << setSize << "/" << curLayer.getNumberOfFunctionInputs() << "\n";
			iterCollectionInParallel(curLayer[setSize], [&](EqClass<IntervalSize>& curClass) {
				curClass.intervalSizeToBottom = computeIntervalSizeToBottom(decomp, curLayerIndex, curClass, memoMap) + 1;
			});
		}
	}
}

valueInt IntervalSize::getDedekind(const DedekindDecomposition<IntervalSize>& decomp) {
	return decomp.fullTop().intervalSizeToBottom;
}
