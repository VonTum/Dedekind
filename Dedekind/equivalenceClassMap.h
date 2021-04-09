#pragma once

#include "equivalenceClass.h"
#include "heapArray.h"

#include "bufferedMap.h"

template<typename V>
using EquivalenceClassMap = BufferedMap<EquivalenceClass, V>;

template<typename Value>
struct BakedEquivalenceClass : public Value {
	EquivalenceClass eqClass;
};

template<typename V>
class BakedEquivalenceClassMap {
public:
	BakedEquivalenceClass<V>* allClasses;
	size_t numberOfClasses;
	BakedEquivalenceClass<V>** buckets;
	size_t bucketCount;
public:
	BakedEquivalenceClassMap() = default;
	BakedEquivalenceClassMap(BakedEquivalenceClass<V>* classesBuf, size_t numberOfClasses, size_t bucketCount) :
		allClasses(classesBuf),
		numberOfClasses(numberOfClasses),
		buckets(new BakedEquivalenceClass<V>*[bucketCount+1]),
		bucketCount(bucketCount) {
		buckets[bucketCount] = allClasses + numberOfClasses;
	}

	BakedEquivalenceClassMap(BakedEquivalenceClassMap&& other) noexcept : 
		allClasses(other.allClasses), 
		numberOfClasses(other.numberOfClasses),
		buckets(other.buckets), 
		bucketCount(other.bucketCount) {

		other.buckets = nullptr;
		other.bucketCount = 0;
	}
	BakedEquivalenceClassMap& operator=(BakedEquivalenceClassMap&& other) noexcept {
		std::swap(this->allClasses, other.allClasses);
		std::swap(this->numberOfClasses, other.numberOfClasses);
		std::swap(this->buckets, other.buckets);
		std::swap(this->bucketCount, other.bucketCount);
	}
	BakedEquivalenceClassMap(const BakedEquivalenceClassMap&) = delete;
	BakedEquivalenceClassMap& operator=(const BakedEquivalenceClassMap&) = delete;

	~BakedEquivalenceClassMap() {
		delete[] buckets;
	}


	BakedEquivalenceClass<V>& get(const PreprocessedFunctionInputSet& f) {
		size_t index = f.hash() % bucketCount;

		BakedEquivalenceClass<V>* b = buckets[index];
		BakedEquivalenceClass<V>* bEnd = buckets[index+1];

		for(; b != bEnd; b++) {
			if(b->eqClass.contains(f)) {
				return *b;
			}
		}
		assert(false); // Cannot happen, this should be a full decomposition of the entire layer
	}
	const BakedEquivalenceClass<V>& get(const PreprocessedFunctionInputSet& f) const {
		size_t index = f.hash() % bucketCount;

		BakedEquivalenceClass<V>* b = buckets[index];
		BakedEquivalenceClass<V>* bEnd = buckets[index + 1];

		for(; b != bEnd; b++) {
			if(b->eqClass.contains(f)) {
				return *b;
			}
		}
		assert(false); // Cannot happen, this should be a full decomposition of the entire layer
		throw "Unreachable";
	}

	BakedEquivalenceClass<V>& operator[](size_t index) { assert(index >= 0 && index < numberOfClasses); return allClasses[index]; }
	const BakedEquivalenceClass<V>& operator[](size_t index) const { assert(index >= 0 && index < numberOfClasses); return allClasses[index]; }

	size_t size() const { return numberOfClasses; }

	BakedEquivalenceClass<V>* begin() { return allClasses; }
	const BakedEquivalenceClass<V>* begin() const { return allClasses; }
	BakedEquivalenceClass<V>* end() { return allClasses + numberOfClasses; }
	const BakedEquivalenceClass<V>* end() const { return allClasses + numberOfClasses; }

	size_t indexOf(const BakedEquivalenceClass<V>& item) const {
		return &item - allClasses;
	}
	size_t indexOf(const PreprocessedFunctionInputSet& item) const {
		return &this->get(item) - allClasses;
	}
};
