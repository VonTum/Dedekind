#pragma once

#include "equivalenceClass.h"

template<typename V>
struct ValuedEquivalenceClass {
	EquivalenceClass equivClass;
	V value;
};

template<typename V>
class EquivalenceClassMap {
	struct MapNode {
		MapNode* nextNode;
		ValuedEquivalenceClass<V> item;
	};

	MapNode** hashTable;
	size_t buckets;
	size_t itemCount;

	MapNode** getBucketFor(const PreprocessedFunctionInputSet& functionInputSet) const {
		uint64_t hash = functionInputSet.hash();
		return &hashTable[hash % buckets];
	}
	MapNode** getNodeFor(const PreprocessedFunctionInputSet& functionInputSet) const {
		MapNode** cur = getBucketFor(functionInputSet);
		for(; *cur != nullptr; cur = &((*cur)->nextNode)) {
			if((*cur)->item.equivClass.contains(functionInputSet)) {
				return cur;
			}
		}
		return cur;
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
				MapNode** bucket = getBucketFor(curNode->item.equivClass);
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
			rehash(buckets * 2);
		}
	}
public:
	EquivalenceClassMap(size_t buckets) : hashTable(new MapNode*[buckets]), buckets(buckets), itemCount(0) {
		for(size_t i = 0; i < buckets; i++) {
			hashTable[i] = nullptr;
		}
	}
	EquivalenceClassMap() : EquivalenceClassMap(16) {}

	EquivalenceClassMap(EquivalenceClassMap&& other) : hashTable(other.hashTable), buckets(other.buckets), itemCount(other.itemCount) {
		other.hashTable = new MapNode*[16];
		other.buckets = 16;
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

	V& get(const PreprocessedFunctionInputSet& preprocessed) {
		MapNode** foundNode = getNodeFor(preprocessed);
		assert(*foundNode != nullptr);
		return (*foundNode)->item.value;
	}
	const V& get(const PreprocessedFunctionInputSet& preprocessed) const {
		MapNode** foundNode = getNodeFor(preprocessed);
		assert(*foundNode != nullptr);
		return (*foundNode)->item.value;
	}
	ValuedEquivalenceClass<V>& getOrAdd(const PreprocessedFunctionInputSet& preprocessed, const V& defaultForCreate) {
		MapNode** foundNode = getNodeFor(preprocessed);
		MapNode* actualNode = *foundNode;
		if(actualNode == nullptr) {
			actualNode = new MapNode{nullptr, ValuedEquivalenceClass<V>{EquivalenceClass(preprocessed), defaultForCreate}};
			*foundNode = actualNode;
			this->notifyNewItem();
		}
		return actualNode->item;
	}

	ValuedEquivalenceClass<V>& add(const PreprocessedFunctionInputSet& preprocessed, const V& value) {
		MapNode** bucket = getBucketFor(preprocessed);
		*bucket = new MapNode{*bucket, ValuedEquivalenceClass<V>{EquivalenceClass(preprocessed), value}};
		this->notifyNewItem();
		return (*bucket)->item;
	}

	ValuedEquivalenceClass<V>& add(const EquivalenceClass& eqClass, const V& value) {
		MapNode** bucket = getBucketFor(eqClass);
		*bucket = new MapNode{*bucket, ValuedEquivalenceClass<V>{eqClass, value}};
		this->notifyNewItem();
		return (*bucket)->item;
	}

	size_t size() const { return itemCount; }
	void clear() {
		deleteAllNodes();
	}
	~EquivalenceClassMap() {
		deleteAllNodes();
		delete[] hashTable;
		buckets = 0;
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
