#pragma once

#include "equivalenceClass.h"
#include "heapArray.h"

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
		MapNode* nextNode;
		ValuedEquivalenceClass<V> item;

		//MapNode(MapNode* nextNode, const EquivalenceClass& equivClass, const V& value) : nextNode(nextNode), item{equivClass, value} {}
	};

	MapNode** hashTable;
	size_t bucketsIndex;
	size_t buckets;
	size_t itemCount;
private:

	MapNode** getBucketFor(uint64_t hash) const {
		return &hashTable[hash % buckets];
	}
	MapNode** getNodeFor(const PreprocessedFunctionInputSet& functionInputSet, uint64_t hash) const {
		MapNode** cur = getBucketFor(hash);
		for(; *cur != nullptr; cur = &((*cur)->nextNode)) {
			const EquivalenceClass& eqClass = (*cur)->item.equivClass;
			if(eqClass.contains(functionInputSet)) {
				return cur;
			}
		}
		return cur;
	}
	MapNode** getNodeFor(const PreprocessedFunctionInputSet& functionInputSet) const {
		return getNodeFor(functionInputSet, functionInputSet.hash());
	}
	void deleteAllNodes() {
		for(size_t i = 0; i < buckets; i++) {
			for(MapNode* curNode = hashTable[i]; curNode != nullptr; ) {
				MapNode* nodeToDelete = curNode;
				curNode = curNode->nextNode;
				delete nodeToDelete;
			}
			hashTable[i] = nullptr;
		}
		itemCount = 0;
	}
	void rehash(size_t newBuckets) {
		MapNode** oldHashTable = this->hashTable;
		size_t oldBuckets = this->buckets;
		this->hashTable = new MapNode*[newBuckets];
		this->buckets = newBuckets;
		for(size_t i = 0; i < this->buckets; i++) this->hashTable[i] = nullptr;
		for(size_t i = 0; i < oldBuckets; i++) {
			for(MapNode* curNode = oldHashTable[i]; curNode != nullptr; ) {
				MapNode** bucket = getBucketFor(curNode->item.equivClass.hash());
				MapNode* nextNode = curNode->nextNode;
				curNode->nextNode = *bucket;
				*bucket = curNode;
				curNode = nextNode;
			}
		}
		delete[] oldHashTable;
	}
	void notifyNewItem() {
		itemCount++;

		if(itemCount * 3 >= buckets) {
			size_t newBuckets = hashMapPrimes[++bucketsIndex];
			rehash(newBuckets);
		}
	}
	EquivalenceClassMap(size_t buckets) : hashTable(new MapNode* [buckets]), buckets(buckets), bucketsIndex(0), itemCount(0) {
		for(size_t i = 0; i < buckets; i++) {
			hashTable[i] = nullptr;
		}
	}
public:
	EquivalenceClassMap() : EquivalenceClassMap(INITIAL_BUCKETS) {}

	EquivalenceClassMap(EquivalenceClassMap&& other) : hashTable(other.hashTable), buckets(other.buckets), bucketsIndex(0), itemCount(other.itemCount) {
		other.hashTable = new MapNode*[INITIAL_BUCKETS];
		other.buckets = INITIAL_BUCKETS;
		other.bucketsIndex = 0;
		other.itemCount = 0;
	}
	EquivalenceClassMap& operator=(EquivalenceClassMap&& other) noexcept {
		std::swap(this->hashTable, other.hashTable);
		std::swap(this->buckets, other.buckets);
		std::swap(this->itemCount, other.itemCount);
		return *this;
	}
	EquivalenceClassMap(const EquivalenceClassMap& other) : EquivalenceClassMap(other.buckets) {
		itemCount = other.itemCount;
		for(size_t i = 0; i < buckets; i++) {
			MapNode** curNodeInNewMap = &(this->hashTable[i]);
			for(MapNode* curNode = other.hashTable[i]; curNode != nullptr; curNode = curNode->nextNode) {
				*curNodeInNewMap = new MapNode(*curNode);
				curNodeInNewMap = &(*curNodeInNewMap)->nextNode;
			}
		}
	}
	EquivalenceClassMap& operator=(const EquivalenceClassMap& other) {
		*this = EquivalenceClassMap(other);
		return *this;
	}
	~EquivalenceClassMap() {
		deleteAllNodes();
		delete[] hashTable;
		buckets = 0;
	}

	ValuedEquivalenceClass<V>* find(const PreprocessedFunctionInputSet& preprocessed, uint64_t hash) {
		MapNode** foundNode = getNodeFor(preprocessed, hash);
		if(*foundNode == nullptr) {
			return nullptr;
		} else {
			return &(**foundNode).item;
		}
	}
	V& get(const PreprocessedFunctionInputSet& preprocessed) {
		MapNode** foundNode = getNodeFor(preprocessed);
		assert(*foundNode != nullptr);
		return (**foundNode).item.value;
	}
	const V& get(const PreprocessedFunctionInputSet& preprocessed) const {
		MapNode** foundNode = getNodeFor(preprocessed);
		assert(*foundNode != nullptr);
		return (**foundNode).item.value;
	}
	ValuedEquivalenceClass<V>& getOrAdd(const PreprocessedFunctionInputSet& preprocessed, const V& defaultForCreate, uint64_t hash) {
		MapNode** foundNode = getNodeFor(preprocessed, hash);
		MapNode* actualNode = *foundNode;
		if(actualNode == nullptr) {
			actualNode = new MapNode{nullptr, {EquivalenceClass(preprocessed), defaultForCreate}};
			*foundNode = actualNode;
			this->notifyNewItem();
		}
		return actualNode->item;
	}
	ValuedEquivalenceClass<V>& getOrAdd(const PreprocessedFunctionInputSet& preprocessed, const V& defaultForCreate) {
		uint64_t hash = preprocessed.hash();
		return getOrAdd(preprocessed, defaultForCreate, hash);
	}

	void findAllNodesMatchingHash(uint64_t hash, std::vector<ValuedEquivalenceClass<V>*>& foundNodes) {
		for(MapNode** cur = getBucketFor(hash); *cur != nullptr; cur = &((*cur)->nextNode)) {
			const EquivalenceClass& eqClass = (*cur)->item.equivClass;
			if(hash == eqClass.hash) {
				foundNodes.push_back(&(*cur)->item);
			}
		}
	}

	ValuedEquivalenceClass<V>& add(const PreprocessedFunctionInputSet& preprocessed, const V& value) {
		uint64_t hash = preprocessed.hash();
		MapNode** bucket = getBucketFor(hash);
		*bucket = new MapNode{*bucket, {EquivalenceClass(preprocessed), value}};
		this->notifyNewItem();
		return (*bucket)->item;
	}

	ValuedEquivalenceClass<V>& add(const EquivalenceClass& eqClass, const V& value) {
		MapNode** bucket = getBucketFor(eqClass.hash());
		*bucket = new MapNode{*bucket, {eqClass, value}};
		this->notifyNewItem();
		return (*bucket)->item;
	}

	size_t size() const { return itemCount; }
	void clear() {
		deleteAllNodes();
	}

	struct EquivalenceClassMapIter {
	protected:
		MapNode** curBucket;
		MapNode** hashTableEnd;
		MapNode* curNode;

	public:
		EquivalenceClassMapIter(MapNode** hashTable, size_t buckets) : curBucket(hashTable), hashTableEnd(hashTable + buckets) {
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
