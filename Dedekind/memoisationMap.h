#pragma once

#include "keyValue.h"



template<typename K, typename V>
class MemoisationMap {
	struct Node {
		KeyValue<K, V> kv;
		Node* next;
	};
	Node* buf;
	Node** hashTable;
	Node*** usedBuckets;

	size_t buckets;
	size_t itemCount = 0;
	size_t usedBucketCount = 0;

public:
	MemoisationMap(size_t capacity, size_t buckets) : buf(new Node[capacity]), hashTable(new Node*[buckets]), usedBuckets(new Node**[capacity]), buckets(buckets) {
		for(size_t i = 0; i < buckets; i++) {
			hashTable[i] = nullptr;
		}
	}
	MemoisationMap(size_t capacity) : MemoisationMap(capacity, getNumberOfBucketsForSize(capacity)) {}
	MemoisationMap() : buf(nullptr), hashTable(nullptr), usedBuckets(nullptr), buckets(0) {}
	~MemoisationMap() {
		delete[] buf;
		delete[] hashTable;
		delete[] usedBuckets;
	}

	MemoisationMap(MemoisationMap&& other) noexcept : 
		buf(other.buf), 
		hashTable(other.hashTable), 
		usedBuckets(other.usedBuckets), 
		buckets(other.buckets), 
		itemCount(other.itemCount), 
		usedBucketCount(other.usedBucketCount) {
		other.buf = nullptr;
		other.hashTable = nullptr;
		other.usedBuckets = nullptr;
		other.buckets = 0;
		other.itemCount = 0;
		other.usedBucketCount = 0;
	}
	MemoisationMap& operator=(MemoisationMap&& other) noexcept {
		std::swap(buf, other.buf);
		std::swap(hashTable, other.hashTable);
		std::swap(buckets, other.buckets);
		std::swap(itemCount, other.itemCount);
		std::swap(usedBucketCount, other.usedBucketCount);
	}

	void rehash(size_t newBuckets) {

	}

	void insert(const K& key, const V& value) {
		size_t bucket = key.hash() % this->buckets;

		Node*& foundBucket = this->hashTable[bucket];

		Node* existingObjectInNodeTable = foundBucket;

		if(existingObjectInNodeTable == nullptr) {
			usedBuckets[usedBucketCount++] = &foundBucket;
		}

		buf[itemCount] = Node{{key, value}, existingObjectInNodeTable};

		foundBucket = &buf[itemCount];

		itemCount++;
	}

	V* get(const K& key) const {
		Node* curNode = hashTable[key.hash() % buckets];
		while(curNode != nullptr) {
			if(curNode->kv.key == key) {
				return &curNode->kv.value;
			}
			curNode = curNode->next;
		}
		return nullptr;
	}

	void clear() {
		for(size_t i = 0; i < usedBucketCount; i++) {
			Node** bucketToReset = usedBuckets[i];

			*bucketToReset = nullptr;
		}

		itemCount = 0;
		usedBucketCount = 0;

#ifndef NDEBUG 
		for(size_t b = 0; b < buckets; b++) {
			assert(hashTable[b] == nullptr);
		}
#endif
	}
};

