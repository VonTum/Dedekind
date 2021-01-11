#pragma once

#include "equivalenceClass.h"
#include "heapArray.h"

#include <atomic>

template<typename V>
struct ValuedEquivalenceClass {
	EquivalenceClass equivClass;
	V value;
};

constexpr size_t hashMapPrimes[]{
	53,97,193,389,769,1543,3079,6151,12289,24593,
	49157,98317,196613,393241,786433,1572869,3145739,6291469,
	12582917,25165843,50331653,100663319,201326611,402653189,805306457,1610612741
};

constexpr size_t INITIAL_BUCKETS = 53;

template<typename V>
class EquivalenceClassMap {
public:
	struct MapNode {
		ValuedEquivalenceClass<V> item;
		std::atomic<MapNode*> nextNode;
	};

	std::atomic<MapNode*>* hashTable;
	MapNode* nodeBuffer;
	size_t buckets;
	size_t itemCount;
private:

	// These are NOT locked, locking should be done by CALLER
	std::atomic<MapNode*>* getBucketFor(uint64_t hash) const {
		return hashTable + hash % buckets;
	}
	std::atomic<MapNode*>* getNodeFor(const PreprocessedFunctionInputSet& functionInputSet, uint64_t hash) const {
		std::atomic<MapNode*>* cur = getBucketFor(hash);
		for(; *cur != nullptr; cur = &(cur->load()->nextNode)) {
			const EquivalenceClass& eqClass = cur->load()->item.equivClass;
			if(eqClass.contains(functionInputSet)) {
				return cur;
			}
		}
		return cur;
	}
	std::atomic<MapNode*>* getNodeFor(const PreprocessedFunctionInputSet& functionInputSet) const {
		return getNodeFor(functionInputSet, functionInputSet.hash());
	}

	// These are protected by bucketLock
	MapNode* getReadOnlyBucket(uint64_t hash) const {
		//shared_lock_guard lg(bucketLock);
		return hashTable[hash % buckets].load();
	}
	MapNode* getReadOnlyNode(const PreprocessedFunctionInputSet& functionInputSet, uint64_t hash) const {
		MapNode* curNode = getReadOnlyBucket(hash);
		while(curNode != nullptr) {
			if(curNode->item.equivClass.contains(functionInputSet)) {
				return curNode;
			} else {
				curNode = curNode->nextNode.load();
			}
		}
		return nullptr;
	}
	MapNode* getReadOnlyNode(const PreprocessedFunctionInputSet& fis) const {
		return getReadOnlyNode(fis, fis.hash());
	}

	void rehash(size_t newBuckets) {
		//std::lock_guard<std::mutex> lg(bucketLock);
		std::atomic<MapNode*>* oldHashTable = this->hashTable;
		size_t oldBuckets = this->buckets;
		this->hashTable = new std::atomic<MapNode*>[newBuckets];
		this->buckets = newBuckets;
		for(size_t i = 0; i < this->buckets; i++) this->hashTable[i] = nullptr;
		for(size_t i = 0; i < oldBuckets; i++) {
			for(MapNode* curNode = oldHashTable[i]; curNode != nullptr; ) {
				std::atomic<MapNode*>& bucket = hashTable[curNode->item.equivClass.hash() % buckets];
				MapNode* nextNode = curNode->nextNode.load();
				MapNode* oldBucketNode = bucket.exchange(curNode);
				curNode->nextNode.store(oldBucketNode);
				curNode = nextNode;
			}
		}
		delete[] oldHashTable;
	}
public:
	void shrink() {
		size_t bestBucketSize;
		for(const size_t& bp : hashMapPrimes) {
			if(bp > this->itemCount * 2) {
				bestBucketSize = bp;
				break;
			}
		}
		bestBucketSize = this->itemCount * 2; // too large, use builtin value
		rehash(bestBucketSize);

	}
	static size_t getNumberOfBucketsForSize(size_t size) {
		size_t lastBp;
		for(const size_t& bp : hashMapPrimes) {
			if(bp > size * 3) {
				return bp;
			}
			lastBp = bp;
		}
		return lastBp;
	}
	void clear() {
		for(size_t i = 0; i < this->itemCount; i++) {
			this->nodeBuffer[i].~MapNode();
		}
		for(size_t i = 0; i < this->buckets; i++) {
			this->hashTable[i] = nullptr;
		}
	}
	void reserveWithClear(size_t newSize) {
		this->clear();
		delete[] this->nodeBuffer;
		this->nodeBuffer = new MapNode[newSize];
		delete[] this->hashTable;
		this->buckets = getNumberOfBucketsForSize(newSize);
		this->hashTable = new std::atomic<MapNode*>[this->buckets];
		for(size_t i = 0; i < this->buckets; i++) {
			this->hashTable[i] = nullptr;
		}
	}
	EquivalenceClassMap(size_t capacity) : hashTable(new std::atomic<MapNode*>[getNumberOfBucketsForSize(capacity)]), buckets(getNumberOfBucketsForSize(capacity)), nodeBuffer(new MapNode[capacity]), itemCount(0) {
		for(size_t i = 0; i < this->buckets; i++) {
			hashTable[i] = nullptr;
		}
	}
	EquivalenceClassMap() : hashTable(nullptr), buckets(0), nodeBuffer(nullptr), itemCount(0) {}

	EquivalenceClassMap(EquivalenceClassMap&& other) : hashTable(other.hashTable), buckets(other.buckets), nodeBuffer(other.nodeBuffer), itemCount(other.itemCount) {
		other.hashTable = nullptr;
		other.buckets = 0;
		other.nodeBuffer = nullptr;
		other.itemCount = 0;
	}
	EquivalenceClassMap& operator=(EquivalenceClassMap&& other) noexcept {
		std::swap(this->hashTable, other.hashTable);
		std::swap(this->buckets, other.buckets);
		std::swap(this->nodeBuffer, other.nodeBuffer);
		std::swap(this->itemCount, other.itemCount);
		return *this;
	}

	~EquivalenceClassMap() {
		this->clear();
		delete[] hashTable;
		buckets = 0;
		delete[] nodeBuffer;
		itemCount = 0;
	}

	ValuedEquivalenceClass<V>* find(const PreprocessedFunctionInputSet& preprocessed, uint64_t hash) {
		MapNode* foundNode = getReadOnlyNode(preprocessed, hash);
		if(foundNode == nullptr) {
			return nullptr;
		} else {
			return &foundNode->item;
		}
	}
	V& get(const PreprocessedFunctionInputSet& preprocessed) {
		MapNode* foundNode = getReadOnlyNode(preprocessed);
		assert(foundNode != nullptr);
		return foundNode->item.value;
	}
	const V& get(const PreprocessedFunctionInputSet& preprocessed) const {
		MapNode* foundNode = getReadOnlyNode(preprocessed);
		assert(foundNode != nullptr);
		return foundNode->item.value;
	}
	ValuedEquivalenceClass<V>& getOrAdd(const PreprocessedFunctionInputSet& preprocessed, const V& defaultForCreate, uint64_t hash) {
		std::atomic<MapNode*>* foundNode = getNodeFor(preprocessed, hash);
		MapNode* actualNode = *foundNode;
		if(actualNode == nullptr) {
			actualNode = this->nodeBuffer + this->itemCount;
			new(actualNode) MapNode{{EquivalenceClass(preprocessed), defaultForCreate}, nullptr};
			foundNode->store(actualNode);
			this->itemCount++;
		}
		return actualNode->item;
	}
	ValuedEquivalenceClass<V>& getOrAdd(const PreprocessedFunctionInputSet& preprocessed, const V& defaultForCreate) {
		uint64_t hash = preprocessed.hash();
		return getOrAdd(preprocessed, defaultForCreate, hash);
	}

	ValuedEquivalenceClass<V>& add(const EquivalenceClass& eqClass, const V& value, uint64_t hash) {
		std::atomic<MapNode*>& bucket = *getBucketFor(hash);
		MapNode* newNode = this->nodeBuffer + this->itemCount;
		new(newNode) MapNode{{eqClass, value}, bucket.load()};
		bucket.store(newNode);
		this->itemCount++;
		return newNode->item;
	}

	ValuedEquivalenceClass<V>& add(const EquivalenceClass& eqClass, const V& value) {
		return this->add(eqClass, value, eqClass.hash());
	}

	ValuedEquivalenceClass<V>& add(const PreprocessedFunctionInputSet& preprocessed, const V& value) {
		EquivalenceClass eqClass = EquivalenceClass(preprocessed);
		return this->add(eqClass, value, eqClass.hash());
	}

	size_t size() const { return itemCount; }
	
	struct EquivalenceClassMapIter {
	protected:
		std::atomic<MapNode*>* curBucket;
		std::atomic<MapNode*>* hashTableEnd;
		MapNode* curNode;

	public:
		EquivalenceClassMapIter(std::atomic<MapNode*>* hashTable, size_t buckets) : curBucket(hashTable), hashTableEnd(hashTable + buckets) {
			while(*curBucket == nullptr && curBucket != hashTableEnd) {
				curBucket++;
			}
			curNode = *curBucket;
		}

		bool operator!=(IteratorEnd) const {
			return curBucket != hashTableEnd;
		}
		void operator++() {
			curNode = curNode->nextNode;
			if(curNode == nullptr) {
				do {
					curBucket++;
				} while(*curBucket == nullptr && curBucket != hashTableEnd);
				curNode = *curBucket;
			}
		}
	};

	struct NonConstEquivalenceClassMapIter : public EquivalenceClassMapIter {
		using EquivalenceClassMapIter::EquivalenceClassMapIter;

		ValuedEquivalenceClass<V>& operator*() const {
			return EquivalenceClassMapIter::curNode->item;
		}
	};
	struct ConstEquivalenceClassMapIter : public EquivalenceClassMapIter {
		using EquivalenceClassMapIter::EquivalenceClassMapIter;

		const ValuedEquivalenceClass<V>& operator*() const {
			return EquivalenceClassMapIter::curNode->item;
		}
	};

	auto begin() { return NonConstEquivalenceClassMapIter(hashTable, buckets); }
	auto begin() const { return ConstEquivalenceClassMapIter(hashTable, buckets); }
	auto end() const { return IteratorEnd(); }
};

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
