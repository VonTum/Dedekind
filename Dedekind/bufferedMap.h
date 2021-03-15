#pragma once

#include <atomic>
#include <algorithm>

#include "keyValue.h"

template<typename T>
class HashBase {
public:
	constexpr static unsigned int BAD_INDEX = ~static_cast<unsigned int>(0);

	std::atomic<unsigned int>* hashTable;
	std::atomic<unsigned int>* nextNodeBuffer;
	size_t buckets;
	T* data;
	size_t itemCount;

public:
	// These are NOT locked, locking should be done by CALLER
	std::atomic<unsigned int>* getBucketFor(uint64_t hash) const {
		return hashTable + hash % buckets;
	}
	template<typename KeyType>
	std::pair<T*, std::atomic<unsigned int>*> getNodeFor(const KeyType& lookingFor, uint64_t hash) const {
		std::atomic<unsigned int>* cur = getBucketFor(hash);
		unsigned int index = cur->load();
		for(; index != BAD_INDEX; index = nextNodeBuffer[index].load()) {
			T& item = data[index];
			if(item == lookingFor) {
				return std::make_pair(&item, cur);
			}
			cur = &nextNodeBuffer[index];
		}
		return std::make_pair(nullptr, cur);
	}
	template<typename KeyType>
	std::pair<T*, std::atomic<unsigned int>*> getNodeFor(const KeyType& lookingFor) const {
		return getNodeFor(lookingFor, lookingFor.hash());
	}

	// These are protected by bucketLock
	unsigned int getReadOnlyBucket(uint64_t hash) const {
		//shared_lock_guard lg(bucketLock);
		return hashTable[hash % buckets].load();
	}
	template<typename KeyType>
	T* getReadOnlyNode(const KeyType& lookingFor, uint64_t hash) const {
		unsigned int curIndex = getReadOnlyBucket(hash);
		while(curIndex != BAD_INDEX) {
			T& item = data[curIndex];
			if(item == lookingFor) {
				return &item;
			} else {
				curIndex = nextNodeBuffer[curIndex].load();
			}
		}
		return nullptr;
	}
	template<typename KeyType>
	T* getReadOnlyNode(const KeyType& lookingFor) const {
		return getReadOnlyNode(lookingFor, lookingFor.hash());
	}

	void rehash(size_t newBuckets) {
		//std::lock_guard<std::mutex> lg(bucketLock);
		std::atomic<unsigned int>* oldHashTable = this->hashTable;
		size_t oldBuckets = this->buckets;
		this->hashTable = new std::atomic<unsigned int>[newBuckets];
		this->buckets = newBuckets;
		for(size_t i = 0; i < this->buckets; i++) this->hashTable[i].store(BAD_INDEX);
		for(size_t i = 0; i < oldBuckets; i++) {
			for(unsigned int curNode = oldHashTable[i].load(); curNode != BAD_INDEX; ) {
				std::atomic<unsigned int>& bucket = hashTable[this->data[curNode].hash() % buckets];
				unsigned int nextNode = nextNodeBuffer[curNode].load();
				unsigned int oldBucketNode = bucket.exchange(curNode);
				nextNodeBuffer[curNode].store(oldBucketNode);
				curNode = nextNode;
			}
		}
		delete[] oldHashTable;
	}
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
	void clear() {
		for(size_t i = 0; i < this->itemCount; i++) {
			this->data[i].~T();
		}
		for(size_t i = 0; i < this->itemCount; i++) {
			this->nextNodeBuffer[i].store(BAD_INDEX);
		}
		for(size_t i = 0; i < this->buckets; i++) {
			this->hashTable[i].store(BAD_INDEX);
		}
		this->itemCount = 0;
	}
	void resizeWithClear(size_t newSize) {
		this->clear();
		delete[] this->nextNodeBuffer;
		this->nextNodeBuffer = new std::atomic<unsigned int>[newSize];
		delete[] this->data;
		this->data = new T[newSize];
		delete[] this->hashTable;
		this->buckets = getNumberOfBucketsForSize(newSize);
		this->hashTable = new std::atomic<unsigned int>[this->buckets];
		for(size_t i = 0; i < this->buckets; i++) {
			this->hashTable[i].store(BAD_INDEX);
		}
		for(size_t i = 0; i < newSize; i++) {
			this->nextNodeBuffer[i].store(BAD_INDEX);
		}
	}
	HashBase(size_t capacity) : hashTable(new std::atomic<unsigned int>[getNumberOfBucketsForSize(capacity)]), nextNodeBuffer(new std::atomic<unsigned int>[capacity]), data(new T[capacity]), buckets(getNumberOfBucketsForSize(capacity)), itemCount(0) {
		for(size_t i = 0; i < this->buckets; i++) {
			this->hashTable[i].store(BAD_INDEX);
		}
		for(size_t i = 0; i < this->itemCount; i++) {
			this->nextNodeBuffer[i].store(BAD_INDEX);
		}
	}
	HashBase() : hashTable(nullptr), nextNodeBuffer(nullptr), data(nullptr), buckets(0), itemCount(0) {}

	HashBase(HashBase&& other) noexcept : hashTable(other.hashTable), nextNodeBuffer(other.nextNodeBuffer), data(other.data), buckets(other.buckets), itemCount(other.itemCount) {
		other.hashTable = nullptr;
		other.buckets = 0;
		other.nextNodeBuffer = nullptr;
		other.data = nullptr;
		other.itemCount = 0;
	}
	HashBase& operator=(HashBase&& other) noexcept {
		std::swap(this->hashTable, other.hashTable);
		std::swap(this->buckets, other.buckets);
		std::swap(this->nextNodeBuffer, other.nextNodeBuffer);
		std::swap(this->data, other.data);
		std::swap(this->itemCount, other.itemCount);
		return *this;
	}

	~HashBase() {
		this->clear();
		delete[] hashTable;
		delete[] nextNodeBuffer;
		delete[] data;
		buckets = 0;
		itemCount = 0;
	}

	T& operator[](size_t index) { return data[index]; }
	const T& operator[](size_t index) const { return data[index]; }

	size_t size() const { return itemCount; }

	template<typename KeyType>
	T* find(const KeyType& key, uint64_t hash) {
		return getReadOnlyNode(key, hash);
	}
	template<typename KeyType>
	T* find(const KeyType& key) {
		return this->find(key, key.hash());
	}
	template<typename KeyType>
	const T* find(const KeyType& key, uint64_t hash) const {
		return getReadOnlyNode(key, hash);
	}
	template<typename KeyType>
	const T* find(const KeyType& key) const {
		return this->find(key, key.hash());
	}

	template<typename KeyType>
	T& get(const KeyType& key) {
		T* foundNode = this->getReadOnlyNode(key);
		assert(foundNode != nullptr);
		return *foundNode;
	}
	template<typename KeyType>
	const T& get(const KeyType& key) const {
		T* foundNode = this->getReadOnlyNode(key);
		assert(foundNode != nullptr);
		return *foundNode;
	}
	T& getOrAdd(const T& item, uint64_t hash) {
		std::pair<T*, std::atomic<unsigned int>*> foundNode = this->getNodeFor(item, hash);
		T* foundKeyValue = foundNode.first;
		std::atomic<unsigned int>& referer = *foundNode.second;
		if(foundKeyValue == nullptr) { // not found
			unsigned int newNodeIdx = this->size();
			data[newNodeIdx] = item;
			nextNodeBuffer[newNodeIdx].store(BAD_INDEX);
			referer.store(newNodeIdx);
			this->itemCount++;
			return data[newNodeIdx];
		} else {
			return *foundKeyValue;
		}
	}
	T& getOrAdd(const T& item) {
		uint64_t hash = item.hash();
		return getOrAdd(item, hash);
	}

	// returns wasAdded
	bool add(const T& item, uint64_t hash) {
		std::atomic<unsigned int>* cur = this->getBucketFor(hash);
		unsigned int index = cur->load();
		while(index != BAD_INDEX) {
			if(this->data[index] == item) {
				return false;
			}
			cur = &this->nextNodeBuffer[index];

			index = cur->load();
		}
		unsigned int newNodeIdx = this->size();
		data[newNodeIdx] = item;
		nextNodeBuffer[newNodeIdx].store(index);
		assert(index == -1 || index < newNodeIdx);
		cur->store(newNodeIdx);
		this->itemCount++;
		return true;
	}

	// returns wasAdded
	bool add(const T& item) {
		return this->add(item, item.hash());
	}

	T* begin() { return data; }
	const T* begin() const { return data; }
	T* end() { return data + itemCount; }
	const T* end() const { return data + itemCount; }
};

template<typename Key, typename Value>
class BufferedMap : public HashBase<KeyValue<Key, Value>> {
public:
	using HashBase<KeyValue<Key, Value>>::HashBase;

	KeyValue<Key, Value>& getOrAdd(const Key& k, const Value& v) {
		return HashBase<KeyValue<Key, Value>>::getOrAdd(KeyValue<Key, Value>{k, v});
	}
	KeyValue<Key, Value>& getOrAdd(const Key& k, const Value& v, uint64_t hash) {
		return HashBase<KeyValue<Key, Value>>::getOrAdd(KeyValue<Key, Value>{k, v}, hash);
	}
	// returns wasAdded
	bool add(const Key& k, const Value& v) {
		return HashBase<KeyValue<Key, Value>>::add(KeyValue<Key, Value>{k, v});
	}
	// returns wasAdded
	bool add(const Key& k, const Value& v, uint64_t hash) {
		return HashBase<KeyValue<Key, Value>>::add(KeyValue<Key, Value>{k, v}, hash);
	}
};

template<typename T>
class BufferedSet : public HashBase<T> {
public:
	using HashBase<T>::HashBase;

	bool contains(const T& item) const {
		return HashBase<T>::find(item) != nullptr;
	}
};

template<typename T>
class BakedHashBase {
	T* data;
	unsigned int* hashTable;
	size_t buckets;
	size_t itemCount;

public:
	BakedHashBase() : hashTable(nullptr), data(nullptr), buckets(0), itemCount(0) {}
	BakedHashBase(T* dataBuffer, size_t size, size_t buckets) : hashTable(new unsigned int[buckets+1]), data(dataBuffer), buckets(buckets), itemCount(size) {
		std::sort(dataBuffer, dataBuffer + size, [buckets](T& a, T& b) {return a.hash() % buckets < b.hash() % buckets; });
		
		unsigned int curBucket = 0;
		uint64_t firstBucket = data[0].hash() % buckets;

		for(; curBucket < firstBucket; curBucket++) {
			this->hashTable[curBucket] = 0;
		}
		this->hashTable[curBucket] = 0;

		for(size_t i = 1; i < size; i++) {
			uint64_t targetBucket = data[i].hash() % buckets;
			assert(targetBucket >= curBucket);

			while(targetBucket > curBucket) {
				curBucket++;
				this->hashTable[curBucket] = i;
				assert(curBucket < buckets + 1);
			}
		}
		assert(curBucket < buckets + 1);
		curBucket++;
		for(; curBucket < buckets + 1; curBucket++) {
			this->hashTable[curBucket] = size;
		}
	}
	BakedHashBase(T* dataBuffer, size_t size) : BakedHashBase(dataBuffer, size, getNumberOfBucketsForSize(size)) {}
	BakedHashBase(const HashBase<T>& from, T* dataBuffer) : hashTable(new unsigned int[from.buckets + 1]), data(dataBuffer), buckets(from.buckets), itemCount(from.itemCount) {
		unsigned int dataIndex = 0;
		for(size_t bucketI = 0; bucketI < from.buckets; bucketI++) {
			this->hashTable[bucketI] = dataIndex;

			for(unsigned int curIndex = from.hashTable[bucketI].load(); curIndex != HashBase<T>::BAD_INDEX; curIndex = from.nextNodeBuffer[curIndex].load()) {
				this->data[dataIndex++] = from.data[curIndex];
			}
		}
		this->hashTable[from.buckets] = dataIndex;
	}

	template<typename OtherT, typename ConvertFunc>
	BakedHashBase(const HashBase<OtherT>& from, T* dataBuffer, const ConvertFunc& convert) : hashTable(new unsigned int[from.buckets + 1]), data(dataBuffer), buckets(from.buckets), itemCount(from.itemCount) {
		unsigned int dataIndex = 0;
		for(size_t bucketI = 0; bucketI < from.buckets; bucketI++) {
			this->hashTable[bucketI] = dataIndex;

			for(unsigned int curIndex = from.hashTable[bucketI].load(); curIndex != HashBase<T>::BAD_INDEX; curIndex = from.nextNodeBuffer[curIndex].load()) {
				this->data[dataIndex++] = convert(from.data[curIndex]);
			}
		}
		this->hashTable[from.buckets] = dataIndex;
	}

	~BakedHashBase() {
		delete[] this->hashTable;
	}

	BakedHashBase(BakedHashBase&& other) noexcept : hashTable(other.hashTable), data(other.data), buckets(other.buckets), itemCount(other.itemCount) {
		other.hashTable = nullptr;
		other.data = nullptr;
		other.buckets = 0;
		other.itemCount = 0;
	}

	BakedHashBase& operator=(BakedHashBase&& other) noexcept {
		std::swap(hashTable, other.hashTable);
		std::swap(data, other.data);
		std::swap(buckets, other.buckets);
		std::swap(itemCount, other.itemCount);
		return *this;
	}

	template<typename KeyType>
	T* find(const KeyType& key) {
		size_t hashIndex = key.hash() % this->buckets;
		unsigned int hashStart = this->hashTable[hashIndex];
		unsigned int hashEnd = this->hashTable[hashIndex+1];

		for(unsigned int curDataIndex = hashStart; curDataIndex < hashEnd; curDataIndex++) {
			T& item = this->data[curDataIndex];
			if(item == key) {
				return &item;
			}
		}

		return nullptr;
	}

	template<typename KeyType>
	const T* find(const KeyType& key) const {
		size_t hashIndex = key.hash() % this->buckets;
		unsigned int hashStart = this->hashTable[hashIndex];
		unsigned int hashEnd = this->hashTable[hashIndex + 1];

		for(unsigned int curDataIndex = hashStart; curDataIndex < hashEnd; curDataIndex++) {
			const T& item = this->data[curDataIndex];
			if(item == key) {
				return &item;
			}
		}

		return nullptr;
	}

	template<typename KeyType>
	T& get(const KeyType& key) {
		T* item = this->find(key);
		assert(item != nullptr);
		return *item;
	}

	template<typename KeyType>
	const T& get(const KeyType& key) const {
		const T* item = this->find(key);
		assert(item != nullptr);
		return *item;
	}

	template<typename KeyType>
	size_t indexOf(const KeyType& key) const {
		const T* item = this->find(key);
		assert(item != nullptr);
		return item - data;
	}

	size_t size() const { return this->itemCount; }

	T& operator[](size_t index) { return data[index]; }
	const T& operator[](size_t index) const { return data[index]; }

	T* begin() { return data; }
	const T* begin() const { return data; }
	T* end() { return data + itemCount; }
	const T* end() const { return data + itemCount; }
};

template<typename Key, typename Value>
class BakedMap : private BakedHashBase<KeyValue<Key, Value>> {
public:
	BakedMap() = default;
	BakedMap(KeyValue<Key, Value>* dataBuffer, size_t size) : BakedHashBase<KeyValue<Key, Value>>(dataBuffer, size) {}
	BakedMap(const BufferedMap<Key, Value>& from, KeyValue<Key, Value>* dataBuffer) : BakedHashBase<KeyValue<Key, Value>>(static_cast<const HashBase<KeyValue<Key, Value>>&>(from), dataBuffer) {}
	BakedMap(const BufferedSet<Key>& from, KeyValue<Key, Value>* dataBuffer) : BakedHashBase<KeyValue<Key, Value>>(static_cast<const HashBase<Key>&>(from), dataBuffer, [](const Key& k) {return KeyValue<Key, Value>{k, {}}; }) {}

	Value& get(const Key& key) {
		return BakedHashBase<KeyValue<Key, Value>>::get(key).value;
	}
	const Value& get(const Key& key) const {
		return BakedHashBase<KeyValue<Key, Value>>::get(key).value;
	}

	Value* find(const Key& key) {
		KeyValue<Key, Value>* found = BakedHashBase<KeyValue<Key, Value>>::find(key);
		if(found != nullptr) {
			return &found->value;
		} else {
			return nullptr;
		}
	}
	const Value* find(const Key& key) const {
		const KeyValue<Key, Value>* found = BakedHashBase<KeyValue<Key, Value>>::find(key);
		if(found != nullptr) {
			return &found->value;
		} else {
			return nullptr;
		}
	}

	bool contains(const Key& key) const {
		return BakedHashBase<KeyValue<Key, Value>>::find(key) != nullptr;
	}

	size_t size() const { return BakedHashBase<KeyValue<Key, Value>>::size(); }
	const KeyValue<Key, Value>& operator[](size_t index) const { return BakedHashBase<KeyValue<Key, Value>>::operator[](index); }
	KeyValue<Key, Value>& operator[](size_t index) { return BakedHashBase<KeyValue<Key, Value>>::operator[](index); }
	const KeyValue<Key, Value>* begin() const { return BakedHashBase<KeyValue<Key, Value>>::begin(); }
	const KeyValue<Key, Value>* end() const { return BakedHashBase<KeyValue<Key, Value>>::end(); }
	KeyValue<Key, Value>* begin() { return BakedHashBase<KeyValue<Key, Value>>::begin(); }
	KeyValue<Key, Value>* end() { return BakedHashBase<KeyValue<Key, Value>>::end(); }
};

template<typename T>
class BakedSet : private BakedHashBase<T> {
public:
	BakedSet() = default;
	BakedSet(T* dataBuffer, size_t size) : BakedHashBase<T>(dataBuffer, size) {}
	BakedSet(const BufferedSet<T>& from, T* dataBuffer) : BakedHashBase<T>(static_cast<const HashBase<T>&>(from), dataBuffer) {}

	bool contains(const T& item) const {
		return BakedHashBase<T>::find(item) != nullptr;
	}

	size_t indexOf(const T& item) const {
		return BakedHashBase<T>::indexOf(item);
	}

	T* find(const T& item) {
		return BakedHashBase<T>::find(item);
	}

	const T* find(const T& item) const {
		return BakedHashBase<T>::find(item);
	}

	size_t size() const { return BakedHashBase<T>::size(); }
	const T& operator[](size_t index) const { return BakedHashBase<T>::operator[](index); }
	T& operator[](size_t index) { return BakedHashBase<T>::operator[](index); }
	const T* begin() const { return BakedHashBase<T>::begin(); }
	const T* end() const { return BakedHashBase<T>::end(); }
	T* begin() { return BakedHashBase<T>::begin(); }
	T* end() { return BakedHashBase<T>::end(); }
};
