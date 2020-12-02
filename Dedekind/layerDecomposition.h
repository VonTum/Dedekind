#pragma once

#include "equivalenceClass.h"
#include "equivalenceClassMap.h"

#include "heapArray.h"
#include "iteratorFactory.h"

#include <vector>


typedef uint64_t bigInt;
typedef bigInt countInt;
typedef bigInt valueInt;

struct EquivalenceClassIndex {
	int size : 8;
	int indexInSubLayer : 24;
	EquivalenceClassIndex() : size(-1), indexInSubLayer(-1) {}
	EquivalenceClassIndex(size_t size, size_t indexInSubLayer) : size(int(size)), indexInSubLayer(int(indexInSubLayer)) {
		assert(indexInSubLayer >= 0 && indexInSubLayer < (1 << 24));
		assert(size >= 0 && size < 256);
	}
};

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
	int inverse;
	EquivalenceClassIndex minimalForcedOnBelow;
	EquivalenceClassIndex minimalForcedOffAbove;

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
/*
struct TestClassThing {};
#define ExtraInfo TestClassThing
*/
template<typename ExtraInfo>
class LayerDecomposition {
	std::vector<BakedEquivalenceClassMap<EquivalenceClassInfo<ExtraInfo>>> equivalenceClasses;
public:

	LayerDecomposition() = default;

	LayerDecomposition(const FullLayer& layer) : equivalenceClasses(layer.size() + 1) {

		std::cout << "Creating Layer decomposition for layer size " << layer.size() << "\n";

		std::vector<EquivalenceClassMap<TempEquivClassInfo>> existingDecomp = createDecomposition(layer);

		std::cout << "Baking: copying over\n";

		// convert to Baked map
		size_t totalConnectionCount = 0; // used for finding the size of the final array which will contain the connections between EquivalenceClasses
		for(size_t i = 0; i < layer.size() + 1; i++) {
			EquivalenceClassMap<TempEquivClassInfo>& extracting = existingDecomp[i];
			BakedEquivalenceClassMap<EquivalenceClassInfo<ExtraInfo>>& newMap = this->equivalenceClasses[i];
			new(&newMap) BakedEquivalenceClassMap<EquivalenceClassInfo<ExtraInfo>>(extracting.size(), extracting.buckets);

			size_t curIndex = 0;
			for(size_t curBucket = 0; curBucket < extracting.buckets; curBucket++) {
				newMap.buckets[curBucket] = newMap.allClasses.ptr() + curIndex;
				for(EquivalenceClassMap<TempEquivClassInfo>::MapNode* cur = extracting.hashTable[curBucket]; cur != nullptr; cur = cur->nextNode) {
					TempEquivClassInfo& oldInfo = cur->item.value;
					new(&newMap[curIndex].eqClass) EquivalenceClass(std::move(cur->item.equivClass));
					curIndex++;
					totalConnectionCount += oldInfo.extendedClasses.size();
					oldInfo.indexInBaked = curIndex;
				}
			}
		}

		std::cout << "Baking: constructing extended lists\n";

		NextClass* nextClassBuf = new NextClass[totalConnectionCount];
		NextClass* curBufIndex = nextClassBuf;
		for(size_t i = 0; i < layer.size() + 1; i++) {
			EquivalenceClassMap<TempEquivClassInfo>& extracting = existingDecomp[i];
			BakedEquivalenceClassMap<EquivalenceClassInfo<ExtraInfo>>& newMap = this->equivalenceClasses[i];

			size_t curIndex = 0;
			for(size_t curBucket = 0; curBucket < extracting.buckets; curBucket++) {
				for(EquivalenceClassMap<TempEquivClassInfo>::MapNode* cur = extracting.hashTable[curBucket]; cur != nullptr; cur = cur->nextNode) {
					TempEquivClassInfo& oldInfo = cur->item.value;
					BakedEquivalenceClass<EquivalenceClassInfo<ExtraInfo>>& cl = newMap[curIndex];
					cl.nextClasses = curBufIndex;
					cl.numberOfNextClasses = oldInfo.extendedClasses.size();
					curBufIndex += oldInfo.extendedClasses.size();
					for(size_t j = 0; j < oldInfo.extendedClasses.size(); j++) {
						NextClass newItem(oldInfo.extendedClasses[j].node->value.indexInBaked, oldInfo.extendedClasses[j].formationCount);
						cl.nextClasses[j] = newItem;
					}
					curIndex++;
				}
			}
		}
		assert(curBufIndex == nextClassBuf + totalConnectionCount);

		std::cout << "Baking: finding inverses\n";
		for(size_t i = 0; i < layer.size() + 1; i++) {
			BakedEquivalenceClassMap<EquivalenceClassInfo<ExtraInfo>>& inverseMap = this->equivalenceClasses[layer.size() - i];
			for(BakedEquivalenceClass<EquivalenceClassInfo<ExtraInfo>>& curClass : this->equivalenceClasses[i]) {
				PreprocessedFunctionInputSet invPrep = preprocess(invert(curClass.eqClass.functionInputSet, layer));
				curClass.inverse = inverseMap.indexOf(invPrep);
			}
		}
		for(size_t i = 0; i < layer.size() + 1; i++) {
			BakedEquivalenceClassMap<EquivalenceClassInfo<ExtraInfo>>& curMap = this->equivalenceClasses[i];
			BakedEquivalenceClassMap<EquivalenceClassInfo<ExtraInfo>>& inverseMap = this->equivalenceClasses[layer.size() - i];
			for(int i = 0; i < curMap.size(); i++) {
				assert(inverseMap[curMap[i].inverse].inverse == i);
			}
		}

		std::cout << "Baking done!\n";

	}

	~LayerDecomposition() {}


	LayerDecomposition(LayerDecomposition&&) = default;
	LayerDecomposition& operator=(LayerDecomposition&&) = default;
	LayerDecomposition(const LayerDecomposition&) = delete;
	LayerDecomposition& operator=(const LayerDecomposition&) = delete;

	inline const BakedEquivalenceClassMap<EquivalenceClassInfo<ExtraInfo>>& operator[](size_t i) const {
		return equivalenceClasses[i];
	}
	inline BakedEquivalenceClassMap<EquivalenceClassInfo<ExtraInfo>>& operator[](size_t i) {
		return equivalenceClasses[i];
	}
	inline BakedEquivalenceClass<EquivalenceClassInfo<ExtraInfo>>& operator[](EquivalenceClassIndex index) {
		BakedEquivalenceClassMap<EquivalenceClassInfo<ExtraInfo>>& map = equivalenceClasses[index.size];
		return map[index.indexInSubLayer];
	}
	inline const BakedEquivalenceClass<EquivalenceClassInfo<ExtraInfo>>& operator[](EquivalenceClassIndex index) const {
		const BakedEquivalenceClassMap<EquivalenceClassInfo<ExtraInfo>>& map = equivalenceClasses[index.size];
		return map[index.indexInSubLayer];
	}

	inline auto begin() { return equivalenceClasses.begin(); }
	inline auto begin() const { return equivalenceClasses.begin(); }
	inline auto end() { return equivalenceClasses.end(); }
	inline auto end() const { return equivalenceClasses.end(); }

	inline BakedEquivalenceClass<EquivalenceClassInfo<ExtraInfo>>& empty() { assert(equivalenceClasses.front().size() == 1); return equivalenceClasses.front()[0]; }
	inline const BakedEquivalenceClass<EquivalenceClassInfo<ExtraInfo>>& empty() const { assert(equivalenceClasses.front().size() == 1); return equivalenceClasses.front()[0]; }
	inline BakedEquivalenceClass<EquivalenceClassInfo<ExtraInfo>>& full() { assert(equivalenceClasses.back().size() == 1); return equivalenceClasses.back()[0]; }
	inline const BakedEquivalenceClass<EquivalenceClassInfo<ExtraInfo>>& full() const { assert(equivalenceClasses.back().size() == 1); return equivalenceClasses.back()[0]; }

	inline size_t getNumberOfFunctionInputs() const { return equivalenceClasses.size() - 1; }

	inline EquivalenceClassIndex indexOf(const PreprocessedFunctionInputSet& prep) const {
		size_t subLayer = prep.functionInputSet.size();
		size_t indexInSubLayer = equivalenceClasses[subLayer].indexOf(prep);

		return EquivalenceClassIndex(subLayer, indexInSubLayer);
	}

	inline BakedEquivalenceClass<EquivalenceClassInfo<ExtraInfo>>& inverseOf(const BakedEquivalenceClass<EquivalenceClassInfo<ExtraInfo>>& eq) {
		int sizeOfInverse = this->getNumberOfFunctionInputs() - eq.eqClass.size();
		return this->equivalenceClasses[sizeOfInverse][eq.inverse];
	}
	inline const BakedEquivalenceClass<EquivalenceClassInfo<ExtraInfo>>& inverseOf(const BakedEquivalenceClass<EquivalenceClassInfo<ExtraInfo>>& eq) const {
		int sizeOfInverse = this->getNumberOfFunctionInputs() - eq.eqClass.size();
		return this->equivalenceClasses[sizeOfInverse][eq.inverse];
	}

	size_t getMaxWidth() const {
		return this->equivalenceClasses[this->equivalenceClasses.size() / 2].size();
	}

	// expects a function void(const BakedEquivalenceClass<EquivalenceClassInfo<ExtraInfo>>& cl, countInt occurences)
	template<typename Func>
	void forEachSuperClassOfClass(int sizeOfStartingNode, int indexOfStartingNode, Func func) const {
		std::vector<int> occurencesOfCurClasses(this->getMaxWidth()+1, 0);
		std::vector<int> occurencesOfNextClasses(this->getMaxWidth()+1, 0);

		std::vector<int> usedCurClasses;
		std::vector<int> usedNextClasses;

		occurencesOfCurClasses[indexOfStartingNode] = 1;
		usedCurClasses.push_back(indexOfStartingNode);

		for(int curSize = sizeOfStartingNode; curSize < this->equivalenceClasses.size(); curSize++) {
			const BakedEquivalenceClassMap<EquivalenceClassInfo<ExtraInfo>>& curMap = this->equivalenceClasses[curSize];
			for(int item : usedCurClasses) {
				const BakedEquivalenceClass<EquivalenceClassInfo<ExtraInfo>>& currentlyPropagating = curMap[item];

				int occurencesOfCurClass = occurencesOfCurClasses[item];
				for(const NextClass& nextClass : currentlyPropagating.iterNextClasses()) {
					bool nextClassIsNewlyDiscoveredClass = (occurencesOfNextClasses[nextClass.nodeIndex] == 0);
					occurencesOfNextClasses[nextClass.nodeIndex] += occurencesOfCurClass * nextClass.formationCount;
					if(nextClassIsNewlyDiscoveredClass) {
						usedNextClasses.push_back(nextClass.nodeIndex);
					}
				}
			}

			// clean up and swap for next iteration
			for(int nextClass : usedNextClasses) {
				func(this->equivalenceClasses[curSize + 1][nextClass], occurencesOfNextClasses[nextClass]);
			}
			for(int curClass : usedCurClasses) {
				occurencesOfCurClasses[curClass] = 0;
			}
			usedCurClasses.clear();
			std::swap(occurencesOfCurClasses, occurencesOfNextClasses);
			std::swap(usedCurClasses, usedNextClasses);
		}
	}

	// expects a function void(const BakedEquivalenceClass<EquivalenceClassInfo<ExtraInfo>>& cl, countInt occurences)
	template<typename Func>
	void forEachSuperClassOfClass(const BakedEquivalenceClass<EquivalenceClassInfo<ExtraInfo>>& node, Func func) const {
		int curSize = node.eqClass.size();
		forEachSuperClassOfClass(curSize, this->equivalenceClasses[curSize].indexOf(node), std::move(func));
	}

	// expects a function void(const BakedEquivalenceClass<EquivalenceClassInfo<ExtraInfo>>& cl, countInt occurences)
	template<typename Func>
	void forEachSubClassOfClass(const BakedEquivalenceClass<EquivalenceClassInfo<ExtraInfo>>& node, Func func) const {
		forEachSuperClassOfClass(this->getNumberOfFunctionInputs() - node.eqClass.size(), node.inverse, [this, func](const BakedEquivalenceClass<EquivalenceClassInfo<ExtraInfo>>& cl, countInt occurences) {
			func(this->inverseOf(cl), occurences);
		});
	}

	// expects a function void(const BakedEquivalenceClass<EquivalenceClassInfo<ExtraInfo>>& cl, countInt occurences)
	template<typename Func>
	void forEachSubClassOfClass(int sizeOfStartingNode, int indexOfStartingNode, Func func) const {
		forEachSubClassOfClass(this->equivalenceClasses[sizeOfStartingNode][indexOfStartingNode]);
	}
};
