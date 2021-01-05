#pragma once

#include "equivalenceClass.h"
#include "equivalenceClassMap.h"

#include "heapArray.h"
#include "iteratorFactory.h"
#include "parallelIter.h"

#include <vector>
#include <assert.h>

typedef int64_t bigInt;
typedef bigInt countInt;
typedef bigInt valueInt;


/*
	Layer Sizes for Dedekind 6:

	2        (1,1)
	7        (1,1,1,1,1,1,1)
	156		 (1,1,2,5,9,15,21,24,24,21,15,9,5,2,1,1)
	2136	 (1,1,3,7,21,43,94,161,249,312,352,312,249,161,94,43,21,7,3,1,1)
	156		 (1,1,2,5,9,15,21,24,24,21,15,9,5,2,1,1)
	7        (1,1,1,1,1,1,1)
	2        (1,1)
*/

/*
	Layer Sizes for Dedekind 7:

	2        (1,1)
	8        (1,1,1,1,1,1,1,1)
	1044     (1,1,2,5,10,21,41,65,97,131,148,148,131,97,65,41,21,10,5,2,1,1)
	7013320  (1,1,3,10,38,137,509,1760,5557,15709,39433,87659,172933,303277,473827,660950,824410,920446,920446,824410,660950,473827,303277,172933,87659,39433,15709,5557,1760,509,137,38,10,3,1,1)
	7013320  (1,1,3,10,38,137,509,1760,5557,15709,39433,87659,172933,303277,473827,660950,824410,920446,920446,824410,660950,473827,303277,172933,87659,39433,15709,5557,1760,509,137,38,10,3,1,1)
	1044     (1,1,2,5,10,21,41,65,97,131,148,148,131,97,65,41,21,10,5,2,1,1)
	8        (1,1,1,1,1,1,1,1)
	2        (1,1)
*/
typedef int32_t eqClassIndexInt;
typedef int32_t inverseIndexInt;

struct NextClass {
	int nodeIndex : 24;
	int formationCount : 8;
	NextClass() : nodeIndex(-1), formationCount(-1) {}
	NextClass(int nodeIndex, int formationCount) : nodeIndex(nodeIndex), formationCount(formationCount) {
		assert(nodeIndex >= 0 && nodeIndex < (1 << 24));
		assert(formationCount >= 0 && formationCount < 256);
	}
};

template<typename Value>
struct EquivalenceClassInfo : public Value {
	NextClass* nextClasses;
	int numberOfNextClasses;
	inverseIndexInt inverse; // 20 bits
	eqClassIndexInt minimalForcedOnBelow; // 23 bits
	eqClassIndexInt minimalForcedOffAbove; // 23 bits

	IteratorFactory<NextClass*> iterNextClasses() {
		return IteratorFactory<NextClass*>(nextClasses, nextClasses + numberOfNextClasses);
	}
	IteratorFactory<const NextClass*> iterNextClasses() const {
		return IteratorFactory<const NextClass*>(nextClasses, nextClasses + numberOfNextClasses);
	}
};

struct TempEquivClassInfo {
	struct AdjacentClass {
		ValuedEquivalenceClass<TempEquivClassInfo>* node;
		countInt formationCount;
	};

	std::vector<AdjacentClass> extendedClasses;
	size_t indexInBaked;
};

std::vector<EquivalenceClassMap<TempEquivClassInfo>> createDecomposition(const FullLayer& layer);


template<typename ExtraData>
using EqClass = BakedEquivalenceClass<EquivalenceClassInfo<ExtraData>>;

/*
struct TestClassThing {};
#define ExtraInfo TestClassThing
*/
template<typename ExtraInfo>
class LayerDecomposition {
	HeapArray<BakedEquivalenceClass<EquivalenceClassInfo<ExtraInfo>>> eqClassBuf;
	std::vector<BakedEquivalenceClassMap<EquivalenceClassInfo<ExtraInfo>>> subSizeMaps;
public:

	LayerDecomposition() = default;

	LayerDecomposition(const FullLayer& layer) : subSizeMaps(layer.size() + 1) {

		std::cout << "Creating Layer decomposition for layer size " << layer.size() << "\n";

		std::vector<EquivalenceClassMap<TempEquivClassInfo>> existingDecomp = createDecomposition(layer);

#ifndef NDEBUG
		for(size_t i = 0; i < existingDecomp.size(); i++) {
			for(ValuedEquivalenceClass<TempEquivClassInfo>& cl : existingDecomp[i]) {
				if(i < existingDecomp.size() - 1) {
					assert(cl.value.extendedClasses.size() >= 1);
				} else {
					assert(cl.value.extendedClasses.size() == 0);
				}
			}
		}
#endif

		size_t totalNumClasses = 0;
		for(size_t i = 0; i < layer.size() + 1; i++) {
			const EquivalenceClassMap<TempEquivClassInfo>& extracting = existingDecomp[i];
			totalNumClasses += extracting.size();
		}
		std::cout << "Baking: Total number of EqClasses: " << totalNumClasses << "\n"; 

		new(&this->eqClassBuf) HeapArray<BakedEquivalenceClass<EquivalenceClassInfo<ExtraInfo>>>(totalNumClasses);
		std::cout << "Baking: copying over\n";

		BakedEquivalenceClass<EquivalenceClassInfo<ExtraInfo>>* curClassBufIndex = this->eqClassBuf.ptr();

		// convert to Baked map
		size_t totalConnectionCount = 0; // used for finding the size of the final array which will contain the connections between EquivalenceClasses
		for(size_t i = 0; i < layer.size() + 1; i++) {
			EquivalenceClassMap<TempEquivClassInfo>& extracting = existingDecomp[i];
			BakedEquivalenceClassMap<EquivalenceClassInfo<ExtraInfo>>& newMap = this->subSizeMaps[i];
			
			new(&newMap) BakedEquivalenceClassMap<EquivalenceClassInfo<ExtraInfo>>(curClassBufIndex, extracting.size(), extracting.buckets);
			curClassBufIndex += extracting.size();


			size_t curIndex = 0;
			for(size_t curBucket = 0; curBucket < extracting.buckets; curBucket++) {
				newMap.buckets[curBucket] = newMap.allClasses + curIndex;
				for(EquivalenceClassMap<TempEquivClassInfo>::MapNode* cur = extracting.hashTable[curBucket]; cur != nullptr; cur = cur->nextNode) {
					TempEquivClassInfo& oldInfo = cur->item.value;
					new(&newMap[curIndex].eqClass) EquivalenceClass(std::move(cur->item.equivClass));
					oldInfo.indexInBaked = curIndex;
					totalConnectionCount += oldInfo.extendedClasses.size();
					curIndex++;
				}
			}
		}
		assert(curClassBufIndex == this->eqClassBuf.end());

		std::cout << "Baking: constructing extended lists\n";

		NextClass* nextClassBuf = new NextClass[totalConnectionCount];
		NextClass* curNextBufIndex = nextClassBuf;
		for(size_t i = 0; i < layer.size() + 1; i++) {
			EquivalenceClassMap<TempEquivClassInfo>& extracting = existingDecomp[i];
			BakedEquivalenceClassMap<EquivalenceClassInfo<ExtraInfo>>& newMap = this->subSizeMaps[i];

			size_t curIndex = 0;
			for(size_t curBucket = 0; curBucket < extracting.buckets; curBucket++) {
				for(EquivalenceClassMap<TempEquivClassInfo>::MapNode* cur = extracting.hashTable[curBucket]; cur != nullptr; cur = cur->nextNode) {
					TempEquivClassInfo& oldInfo = cur->item.value;
					BakedEquivalenceClass<EquivalenceClassInfo<ExtraInfo>>& cl = newMap[curIndex];
					cl.nextClasses = curNextBufIndex;
					cl.numberOfNextClasses = oldInfo.extendedClasses.size();
					curNextBufIndex += oldInfo.extendedClasses.size();
					for(size_t j = 0; j < oldInfo.extendedClasses.size(); j++) {
						NextClass newItem(oldInfo.extendedClasses[j].node->value.indexInBaked, oldInfo.extendedClasses[j].formationCount);
						cl.nextClasses[j] = newItem;
					}
					curIndex++;
				}
			}
		}
		assert(curNextBufIndex == nextClassBuf + totalConnectionCount);

		std::cout << "Baking: finding inverses\n";
		for(size_t i = 0; i < layer.size() + 1; i++) {
			BakedEquivalenceClassMap<EquivalenceClassInfo<ExtraInfo>>& inverseMap = this->subSizeMaps[layer.size() - i];
			iterCollectionInParallel(this->subSizeMaps[i], [&inverseMap, &layer](BakedEquivalenceClass<EquivalenceClassInfo<ExtraInfo>>& curClass) {
				PreprocessedFunctionInputSet invPrep = preprocess(invert(curClass.eqClass.functionInputSet, layer));
				curClass.inverse = inverseMap.indexOf(invPrep);
			});
		}
#ifndef NDEBUG
		for(size_t i = 0; i < layer.size() + 1; i++) {
			BakedEquivalenceClassMap<EquivalenceClassInfo<ExtraInfo>>& curMap = this->subSizeMaps[i];
			BakedEquivalenceClassMap<EquivalenceClassInfo<ExtraInfo>>& inverseMap = this->subSizeMaps[layer.size() - i];
			for(int i = 0; i < curMap.size(); i++) {
				assert(inverseMap[curMap[i].inverse].inverse == i);
			}
		}
#endif
		std::cout << "Baking done!\n";

	}

	~LayerDecomposition() {}


	LayerDecomposition(LayerDecomposition&&) = default;
	LayerDecomposition& operator=(LayerDecomposition&&) = default;
	LayerDecomposition(const LayerDecomposition&) = delete;
	LayerDecomposition& operator=(const LayerDecomposition&) = delete;

	inline const BakedEquivalenceClassMap<EquivalenceClassInfo<ExtraInfo>>& subSizeMap(size_t i) const {
		return subSizeMaps[i];
	}
	inline BakedEquivalenceClassMap<EquivalenceClassInfo<ExtraInfo>>& subSizeMap(size_t i) {
		return subSizeMaps[i];
	}
	inline BakedEquivalenceClass<EquivalenceClassInfo<ExtraInfo>>& operator[](eqClassIndexInt index) {
		return eqClassBuf[index];
	}
	inline const BakedEquivalenceClass<EquivalenceClassInfo<ExtraInfo>>& operator[](eqClassIndexInt index) const {
		return eqClassBuf[index];
	}

	inline auto begin() { return subSizeMaps.begin(); }
	inline auto begin() const { return subSizeMaps.begin(); }
	inline auto end() { return subSizeMaps.end(); }
	inline auto end() const { return subSizeMaps.end(); }

	inline BakedEquivalenceClass<EquivalenceClassInfo<ExtraInfo>>& empty() { assert(subSizeMaps.front().size() == 1); return subSizeMaps.front()[0]; }
	inline const BakedEquivalenceClass<EquivalenceClassInfo<ExtraInfo>>& empty() const { assert(subSizeMaps.front().size() == 1); return subSizeMaps.front()[0]; }
	inline BakedEquivalenceClass<EquivalenceClassInfo<ExtraInfo>>& full() { assert(subSizeMaps.back().size() == 1); return subSizeMaps.back()[0]; }
	inline const BakedEquivalenceClass<EquivalenceClassInfo<ExtraInfo>>& full() const { assert(subSizeMaps.back().size() == 1); return subSizeMaps.back()[0]; }

	inline size_t getNumberOfFunctionInputs() const { return subSizeMaps.size() - 1; }

	inline eqClassIndexInt indexOf(const PreprocessedFunctionInputSet& prep) const {
		size_t subLayer = prep.functionInputSet.size();
		return &subSizeMaps[subLayer].get(prep) - eqClassBuf.ptr();
	}

	inline BakedEquivalenceClass<EquivalenceClassInfo<ExtraInfo>>& inverseOf(const BakedEquivalenceClass<EquivalenceClassInfo<ExtraInfo>>& eq) {
		int sizeOfInverse = this->getNumberOfFunctionInputs() - eq.eqClass.size();
		return this->subSizeMaps[sizeOfInverse][eq.inverse];
	}
	inline const BakedEquivalenceClass<EquivalenceClassInfo<ExtraInfo>>& inverseOf(const BakedEquivalenceClass<EquivalenceClassInfo<ExtraInfo>>& eq) const {
		int sizeOfInverse = this->getNumberOfFunctionInputs() - eq.eqClass.size();
		return this->subSizeMaps[sizeOfInverse][eq.inverse];
	}

	size_t getMaxWidth() const {
		const BakedEquivalenceClassMap<EquivalenceClassInfo<ExtraInfo>>& biggestMap = this->subSizeMaps[this->subSizeMaps.size() / 2];
		return biggestMap.size();
	}

	struct ForEachBuffer {
		std::vector<int> occurencesOfCurClasses;
		std::vector<int> occurencesOfNextClasses;

		std::vector<int> usedCurClasses;
		std::vector<int> usedNextClasses;

		ForEachBuffer(size_t maxNumClasses) : 
			occurencesOfCurClasses(maxNumClasses, 0), 
			occurencesOfNextClasses(maxNumClasses, 0), 
			usedCurClasses(), 
			usedNextClasses() {
			usedCurClasses.reserve(maxNumClasses);
			usedNextClasses.reserve(maxNumClasses);
		}
	};

	inline ForEachBuffer makeForEachBuffer() const {
		return ForEachBuffer(this->getMaxWidth());
	}

	// expects a function void(const BakedEquivalenceClass<EquivalenceClassInfo<ExtraInfo>>& cl, countInt occurences)
	template<typename Func>
	void forEachSuperClassOfClass(int sizeOfStartingNode, int indexOfStartingNode, ForEachBuffer& buf, Func func) const {
		buf.occurencesOfCurClasses[indexOfStartingNode] = 1;
		buf.usedCurClasses.push_back(indexOfStartingNode);

		/*
			Duplicate choices:
			You start at some FunctionInputSet s, with a number of active elements, we add one, so we have one choice of where to put it
			We add a second one, we could have put it at the end, or we could have put it before the first, 2 choices
			and so on
		*/
		int numberOfDuplicateChoices = 1;

		for(int curSize = sizeOfStartingNode; curSize < this->subSizeMaps.size(); curSize++) {
			const BakedEquivalenceClassMap<EquivalenceClassInfo<ExtraInfo>>& curMap = this->subSizeMaps[curSize];
			for(int item : buf.usedCurClasses) {
				const BakedEquivalenceClass<EquivalenceClassInfo<ExtraInfo>>& currentlyPropagating = curMap[item];

				int occurencesOfCurClass = buf.occurencesOfCurClasses[item];
				for(const NextClass& nextClass : currentlyPropagating.iterNextClasses()) {
					bool nextClassIsNewlyDiscoveredClass = (buf.occurencesOfNextClasses[nextClass.nodeIndex] == 0);
					buf.occurencesOfNextClasses[nextClass.nodeIndex] += occurencesOfCurClass * nextClass.formationCount;
					if(nextClassIsNewlyDiscoveredClass) {
						buf.usedNextClasses.push_back(nextClass.nodeIndex);
					}
				}
			}

			// run function on every count that's been found
			for(int nextClass : buf.usedNextClasses) {
				buf.occurencesOfNextClasses[nextClass] /= numberOfDuplicateChoices;
				func(this->subSizeMaps[curSize + 1][nextClass], buf.occurencesOfNextClasses[nextClass]);
			}

			// clean up and swap for next iteration
			for(int curClass : buf.usedCurClasses) {
				buf.occurencesOfCurClasses[curClass] = 0;
			}
			buf.usedCurClasses.clear();
			std::swap(buf.occurencesOfCurClasses, buf.occurencesOfNextClasses);
			std::swap(buf.usedCurClasses, buf.usedNextClasses);

			numberOfDuplicateChoices++;
		}
		for(int curClass : buf.usedCurClasses) {
			buf.occurencesOfCurClasses[curClass] = 0;
		}
		buf.usedCurClasses.clear();
	}
	template<typename Func>
	void forEachSuperClassOfClass(int sizeOfStartingNode, int indexOfStartingNode, Func func) const {
		ForEachBuffer buf = this->makeForEachBuffer();
		forEachSuperClassOfClass(sizeOfStartingNode, indexOfStartingNode, buf, std::move(func));
	}

	// expects a function void(const BakedEquivalenceClass<EquivalenceClassInfo<ExtraInfo>>& cl, countInt occurences)
	template<typename Func>
	void forEachSuperClassOfClass(const BakedEquivalenceClass<EquivalenceClassInfo<ExtraInfo>>& node, Func func) const {
		int curSize = node.eqClass.size();
		forEachSuperClassOfClass(curSize, this->subSizeMaps[curSize].indexOf(node), std::move(func));
	}

	// expects a function void(const BakedEquivalenceClass<EquivalenceClassInfo<ExtraInfo>>& cl, countInt occurences)
	template<typename Func>
	void forEachSubClassOfClass(const BakedEquivalenceClass<EquivalenceClassInfo<ExtraInfo>>& node, Func func) const {
		forEachSuperClassOfClass(this->getNumberOfFunctionInputs() - node.eqClass.size(), node.inverse, [this, &func](const BakedEquivalenceClass<EquivalenceClassInfo<ExtraInfo>>& cl, countInt occurences) {
			func(this->inverseOf(cl), occurences);
		});
	}

	// expects a function void(const BakedEquivalenceClass<EquivalenceClassInfo<ExtraInfo>>& cl, countInt occurences)
	template<typename Func>
	void forEachSubClassOfClass(int sizeOfStartingNode, int indexOfStartingNode, Func func) const {
		forEachSubClassOfClass(this->subSizeMaps[sizeOfStartingNode][indexOfStartingNode], std::move(func));
	}



	// expects a function void(const BakedEquivalenceClass<EquivalenceClassInfo<ExtraInfo>>& cl, countInt occurences)
	template<typename Func>
	void forEachSuperClassOfClass(const BakedEquivalenceClass<EquivalenceClassInfo<ExtraInfo>>& node, ForEachBuffer& buf, Func func) const {
		int curSize = node.eqClass.size();
		forEachSuperClassOfClass(curSize, this->subSizeMaps[curSize].indexOf(node), buf, std::move(func));
	}

	// expects a function void(const BakedEquivalenceClass<EquivalenceClassInfo<ExtraInfo>>& cl, countInt occurences)
	template<typename Func>
	void forEachSubClassOfClass(const BakedEquivalenceClass<EquivalenceClassInfo<ExtraInfo>>& node, ForEachBuffer& buf, Func func) const {
		forEachSuperClassOfClass(this->getNumberOfFunctionInputs() - node.eqClass.size(), node.inverse, buf, [this, &func](const BakedEquivalenceClass<EquivalenceClassInfo<ExtraInfo>>& cl, countInt occurences) {
			func(this->inverseOf(cl), occurences);
		});
	}

	// expects a function void(const BakedEquivalenceClass<EquivalenceClassInfo<ExtraInfo>>& cl, countInt occurences)
	template<typename Func>
	void forEachSubClassOfClass(int sizeOfStartingNode, int indexOfStartingNode, ForEachBuffer& buf, Func func) const {
		forEachSubClassOfClass(this->subSizeMaps[sizeOfStartingNode][indexOfStartingNode], buf, std::move(func));
	}
};
