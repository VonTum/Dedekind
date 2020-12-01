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
	EquivalenceClassIndex(size_t size, size_t indexInSubLayer) : size(size), indexInSubLayer(indexInSubLayer) {
		assert(indexInSubLayer >= 0 && indexInSubLayer < (1 << 24));
		assert(size >= 0 && size < 256);
	}
};

struct EquivalenceClassInfo {
	struct NextClass {
		int nodeIndex : 24;
		int formationCount : 8;
		NextClass() : nodeIndex(-1), formationCount(-1) {}
		NextClass(int nodeIndex, int formationCount) : nodeIndex(nodeIndex), formationCount(formationCount) {
			assert(nodeIndex >= 0 && nodeIndex < (1 << 24));
			assert(formationCount >= 0 && formationCount < 256);
		}
	};

	NextClass* nextClasses;
	int numberOfNextClasses;
	int inverse;
	EquivalenceClassIndex minimalForcedOnBelow;
	EquivalenceClassIndex minimalForcedOffAbove;
	valueInt value;
	countInt count;

	IteratorFactory<NextClass*> iterNextClasses() {
		return IteratorFactory<NextClass*>(nextClasses, nextClasses + numberOfNextClasses);
	}
	IteratorFactory<const NextClass*> iterNextClasses() const {
		return IteratorFactory<const NextClass*>(nextClasses, nextClasses + numberOfNextClasses);
	}
};

typedef BakedEquivalenceClassMap<EquivalenceClassInfo> EquivalenceMap;
typedef BakedValuedEquivalenceClass<EquivalenceClassInfo> EquivalenceNode;


class LayerDecomposition {
	std::vector<EquivalenceMap> equivalenceClasses;
public:

	LayerDecomposition() = default;
	LayerDecomposition(const FullLayer& fullLayer);

	LayerDecomposition(LayerDecomposition&&) = default;
	LayerDecomposition& operator=(LayerDecomposition&&) = default;
	LayerDecomposition(const LayerDecomposition&) = delete;
	LayerDecomposition& operator=(const LayerDecomposition&) = delete;

	inline const EquivalenceMap& operator[](size_t i) const {
		return equivalenceClasses[i];
	}
	inline EquivalenceMap& operator[](size_t i) {
		return equivalenceClasses[i];
	}
	inline EquivalenceNode& operator[](EquivalenceClassIndex index) {
		EquivalenceMap& map = equivalenceClasses[index.size];
		return map[index.indexInSubLayer];
	}
	inline const EquivalenceNode& operator[](EquivalenceClassIndex index) const {
		const EquivalenceMap& map = equivalenceClasses[index.size];
		return map[index.indexInSubLayer];
	}

	inline auto begin() { return equivalenceClasses.begin(); }
	inline auto begin() const { return equivalenceClasses.begin(); }
	inline auto end() { return equivalenceClasses.end(); }
	inline auto end() const { return equivalenceClasses.end(); }

	inline EquivalenceNode& empty() { assert(equivalenceClasses.front().size() == 1); return equivalenceClasses.front()[0]; }
	inline const EquivalenceNode& empty() const { assert(equivalenceClasses.front().size() == 1); return equivalenceClasses.front()[0]; }
	inline EquivalenceNode& full() { assert(equivalenceClasses.back().size() == 1); return equivalenceClasses.back()[0]; }
	inline const EquivalenceNode& full() const { assert(equivalenceClasses.back().size() == 1); return equivalenceClasses.back()[0]; }

	inline size_t getNumberOfFunctionInputs() const { return equivalenceClasses.size() - 1; }

	inline EquivalenceClassIndex indexOf(const PreprocessedFunctionInputSet& prep) const {
		size_t subLayer = prep.functionInputSet.size();
		size_t indexInSubLayer = equivalenceClasses[subLayer].indexOf(prep);

		return EquivalenceClassIndex(subLayer, indexInSubLayer);
	}

	inline EquivalenceNode& inverseOf(const EquivalenceNode& eq) {
		int sizeOfInverse = this->getNumberOfFunctionInputs() - eq.eqClass.size();
		return this->equivalenceClasses[sizeOfInverse][eq.value.inverse];
	}
	inline const EquivalenceNode& inverseOf(const EquivalenceNode& eq) const {
		int sizeOfInverse = this->getNumberOfFunctionInputs() - eq.eqClass.size();
		return this->equivalenceClasses[sizeOfInverse][eq.value.inverse];
	}

	size_t getMaxWidth() const {
		return this->equivalenceClasses[this->equivalenceClasses.size() / 2].size();
	}

	// expects a function void(const EquivalenceNode& cl, countInt occurences)
	template<typename Func>
	void forEachSuperClassOfClass(int sizeOfStartingNode, int indexOfStartingNode, Func func) const {
		std::vector<int> occurencesOfCurClasses(this->getMaxWidth()+1, 0);
		std::vector<int> occurencesOfNextClasses(this->getMaxWidth()+1, 0);

		std::vector<int> usedCurClasses;
		std::vector<int> usedNextClasses;

		occurencesOfCurClasses[indexOfStartingNode] = 1;
		usedCurClasses.push_back(indexOfStartingNode);

		for(int curSize = sizeOfStartingNode; curSize < this->equivalenceClasses.size(); curSize++) {
			const EquivalenceMap& curMap = this->equivalenceClasses[curSize];
			for(int item : usedCurClasses) {
				const EquivalenceNode& currentlyPropagating = curMap[item];

				int occurencesOfCurClass = occurencesOfCurClasses[item];
				for(const EquivalenceClassInfo::NextClass& nextClass : currentlyPropagating.value.iterNextClasses()) {
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

	// expects a function void(const EquivalenceNode& cl, countInt occurences)
	template<typename Func>
	void forEachSuperClassOfClass(const EquivalenceNode& node, Func func) const {
		int curSize = node.eqClass.size();
		forEachSuperClassOfClass(curSize, this->equivalenceClasses[curSize].indexOf(node), std::move(func));
	}

	// expects a function void(const EquivalenceNode& cl, countInt occurences)
	template<typename Func>
	void forEachSubClassOfClass(const EquivalenceNode& node, Func func) const {
		forEachSuperClassOfClass(this->getNumberOfFunctionInputs() - node.eqClass.size(), node.value.inverse, [this, func](const EquivalenceNode& cl, countInt occurences) {
			func(this->inverseOf(cl), occurences);
		});
	}

	// expects a function void(const EquivalenceNode& cl, countInt occurences)
	template<typename Func>
	void forEachSubClassOfClass(int sizeOfStartingNode, int indexOfStartingNode, Func func) const {
		forEachSubClassOfClass(this->equivalenceClasses[sizeOfStartingNode][indexOfStartingNode]);
	}
};
