#include "intervalDecomposition.h"

#include <unordered_map>
#include <shared_mutex>
#include <algorithm>
#include "sharedLockGuard.h"

#include <iostream>
#include "toString.h"

using AvailableChoices = SmallVector<eqClassIndexInt, MAX_PREPROCESSED-1>;
using AvailableChoiceFis = SmallVector<FunctionInputSet, MAX_PREPROCESSED-1>;

static_assert(MAX_PREPROCESSED <= 7, "This implementation is specific to Dedekind 7 and below");
struct CompressedAvailableChoices {
	union {
		uint64_t largePart;
		struct {
			uint64_t layer6 : 4;  // up to 8
			uint64_t layer5 : 12; // up to 1044
			uint64_t layer4 : 24; // up to 7013320
			uint64_t layer3 : 24; // up to 7013320
		};
	};
	union {
		uint16_t smallPart;
		struct {
			uint16_t layer2 : 12; // up to 1044
			uint16_t layer1 : 4;  // up to 8
		};
	};

	CompressedAvailableChoices(uint64_t largePart, uint16_t smallPart) : largePart(largePart), smallPart(smallPart) {}
	CompressedAvailableChoices(eqClassIndexInt layer1, eqClassIndexInt layer2, eqClassIndexInt layer3, eqClassIndexInt layer4, eqClassIndexInt layer5, eqClassIndexInt layer6) :
		layer1(layer1), layer2(layer2), layer3(layer3), layer4(layer4), layer5(layer5), layer6(layer6) {
	
		assert(layer1 < 8);
		assert(layer2 < 1044);
		assert(layer3 < 7013320);
		assert(layer4 < 7013320);
		assert(layer5 < 1044);
		assert(layer6 < 8);
	}
	CompressedAvailableChoices(eqClassIndexInt* layers) : CompressedAvailableChoices(layers[0], layers[1], layers[2], layers[3], layers[4], layers[5]) {}
};


static bool isEntirelyEmpty(const AvailableChoiceFis& baseNonDominatedLayers) {
	for(const FunctionInputSet& fi : baseNonDominatedLayers) {
		if(fi.size() != 0) {
			return false;
		}
	}
	return true;
}

CompressedAvailableChoices produceAvailableChoices(const DedekindDecomposition<IntervalSize>& decomp, const AvailableChoiceFis& availableChoices) {
	eqClassIndexInt indices[MAX_PREPROCESSED - 1];
	for(size_t i = 0; i < availableChoices.size(); i++) {
		indices[i] = decomp[i + 1/*because the bottom layer isn't included*/].indexOf(preprocess(availableChoices[i]));
	}
	for(size_t i = availableChoices.size(); i < MAX_PREPROCESSED - 1; i++) {
		indices[i] = 0;
	}
	return CompressedAvailableChoices(indices);
}

class MemoizedRecursiveChoices {
	struct SmallPartsSlab {
		struct Element {
			uint64_t smallPart : 16;
			uint64_t numChoices : 48;

			Element() : smallPart(0), numChoices(0) {}
			Element(uint16_t smallPart, valueInt numChoices) : smallPart(smallPart), numChoices(numChoices) { assert(numChoices < (uint64_t(1) << 48)); }
		};

		Element* data;
		size_t dataSize;

		SmallPartsSlab(const Element& firstValue) : data(new Element[1]{firstValue}), dataSize(1) {}
		~SmallPartsSlab() { delete[] data; }

		// not needed
		SmallPartsSlab(const SmallPartsSlab& firstValue) = delete;
		SmallPartsSlab& operator=(const SmallPartsSlab& firstValue) = delete;
		SmallPartsSlab(SmallPartsSlab&& firstValue) = delete;
		SmallPartsSlab& operator=(SmallPartsSlab&& firstValue) = delete;

		void insert(const Element& newElement, Element* index) {
			bool isPowerOf2 = ((dataSize & (dataSize - 1)) == 0);
			if(isPowerOf2) {
				Element* oldBuf = this->data;
				Element* newBuf = new Element[dataSize * 2];
				Element* newBufElem = newBuf;
				Element* oldBufElem = oldBuf;
				for(; oldBufElem < index;) {
					*newBufElem++ = *oldBufElem++;
				}
				*newBufElem++ = newElement;
				for(; oldBufElem < oldBuf + dataSize;) {
					*newBufElem++ = *oldBufElem++;
				}
				this->data = newBuf;
				delete[] oldBuf;
			} else {
				for(Element* curItem = data + (dataSize - 1); curItem >= index; curItem--) {
					*(curItem + 1) = *curItem;
				}
				*index = newElement;
			}
			dataSize++;
		}
		const Element* find(uint16_t smallPart) const {
			return std::lower_bound(data, data + dataSize, smallPart, [](const Element& el, uint16_t v) {return el.smallPart < v; });
		}
		Element* find(uint16_t smallPart) {
			return std::lower_bound(data, data + dataSize, smallPart, [](const Element& el, uint16_t v) {return el.smallPart < v; });
		}

		Element* begin() { return data; }
		const Element* begin() const { return data; }
		Element* end() { return data + dataSize; }
		const Element* end() const { return data + dataSize; }
	};
	std::unordered_map<uint64_t, SmallPartsSlab> memoMap;
	mutable std::shared_mutex lock;
public:
	valueInt compute(const DedekindDecomposition<IntervalSize>& decomp, const AvailableChoiceFis& baseNonDominatedLayers);
	valueInt getOrCompute(const DedekindDecomposition<IntervalSize>& decomp, const AvailableChoiceFis& baseNonDominatedLayers);
};

valueInt MemoizedRecursiveChoices::compute(const DedekindDecomposition<IntervalSize>& decomp, const AvailableChoiceFis& baseNonDominatedLayers) {
	valueInt totalChoices = 0;
	forEachSubgroup(baseNonDominatedLayers.back(), [&decomp, &baseNonDominatedLayers, &totalChoices, this](const FunctionInputSet& forcingElems) {
		// the forcing elements are elements that force the elements from the layer below. For example, for monotonic boolean functions going false->true, this would be the false-elements. 
		AvailableChoiceFis availableInThisSplit(baseNonDominatedLayers.size() - 1);
		for(size_t i = 0; i < baseNonDominatedLayers.size() - 1; i++) {
			availableInThisSplit[i] = getNonDominatedElements(baseNonDominatedLayers[i], forcingElems);
		}

		totalChoices += getOrCompute(decomp, availableInThisSplit);
	});
	return totalChoices;
}
valueInt MemoizedRecursiveChoices::getOrCompute(const DedekindDecomposition<IntervalSize>& decomp, const AvailableChoiceFis& baseNonDominatedLayers) {
	if(isEntirelyEmpty(baseNonDominatedLayers)) {
		return 1;
	}

	CompressedAvailableChoices avc = produceAvailableChoices(decomp, baseNonDominatedLayers);
	lock.lock_shared();
	auto iter = memoMap.find(avc.largePart);
	if(iter != memoMap.end()) {
		SmallPartsSlab& slab = (*iter).second;
		SmallPartsSlab::Element* foundElem = slab.find(avc.smallPart);
		if(foundElem != slab.end() && foundElem->smallPart == avc.smallPart) {
			lock.unlock_shared();
			return foundElem->numChoices;
		} // element was not found
		// must be in index form as the slab data may be moved by other threads
		size_t foundIndex = foundElem - slab.data;
		lock.unlock_shared();

		// slab will remain valid even if edited by other threads, see unordered_map/rehash
		valueInt totalChoices = compute(decomp, baseNonDominatedLayers);

		lock.lock();
		foundElem = slab.data + foundIndex; // restore correct foundElem ptr
		for(; foundElem->smallPart < avc.smallPart && foundElem != slab.end(); foundElem++);

		if(foundElem->smallPart == avc.smallPart) {
			assert(foundElem->numChoices == totalChoices);
			lock.unlock();
			return totalChoices;
		}
		// element was not found
		slab.insert(SmallPartsSlab::Element(avc.smallPart, totalChoices), foundElem);
		lock.unlock();
		return totalChoices;
	} else {
		lock.unlock_shared();

		valueInt totalChoices = compute(decomp, baseNonDominatedLayers);
		lock.lock();
		memoMap.emplace(avc.largePart, SmallPartsSlab::Element(avc.smallPart, totalChoices));
		lock.unlock();
		return totalChoices;
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
	return memoMap.getOrCompute(decomp, forcedInPrevLayers);
}

void IntervalSize::populate(DedekindDecomposition<IntervalSize>& decomp) {
	MemoizedRecursiveChoices memoMap;
	// both the bottom and top layers are computed in a special way, so that they do not need to be included in the main computation, saving on memory and complexity
	decomp[0].full().intervalSizeToBottom = 2;
	for(size_t curLayerIndex = 1; curLayerIndex < decomp.numLayers()-1; curLayerIndex++) {
		LayerDecomposition<IntervalSize>& curLayer = decomp[curLayerIndex];
		for(size_t setSize = 1; setSize <= curLayer.getNumberOfFunctionInputs(); setSize++) {
			std::cout << "Assigning interval sizes of " << setSize << "/" << curLayer.getNumberOfFunctionInputs() << "\n";
			iterCollectionInParallel(curLayer.subSizeMap(setSize), [&](EqClass<IntervalSize>& curClass) {
				curClass.intervalSizeToBottom = computeIntervalSizeToBottom(decomp, curLayerIndex, curClass, memoMap) + 1;
			});
		}
	}
	// done manually as it's trivial to compute, and would take a long time / increase memory requirements if explicitly computed / no need to keep track of an entire extra layer
	decomp[decomp.numLayers() - 1].full().intervalSizeToBottom = decomp[decomp.numLayers() - 2].full().intervalSizeToBottom + 1;
}

valueInt IntervalSize::getDedekind(const DedekindDecomposition<IntervalSize>& decomp) {
	return decomp.fullTop().intervalSizeToBottom;
}
