#pragma once

#include <atomic>

constexpr size_t hashMapPrimes[]{
	53,97,193,389,769,1543,3079,6151,12289,24593,
	49157,98317,196613,393241,786433,1572869,3145739,6291469,
	12582917,25165843,50331653,100663319,201326611,402653189,805306457,1610612741
};

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
	static size_t getNumberOfBucketsForSize(size_t size) {
		size_t lastBp;
		for(const size_t& bp : hashMapPrimes) {
			if(bp > size) {
				return bp;
			}
			lastBp = bp;
		}
		return lastBp;
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
struct KeyValue {
	Key key;
	Value value;

	uint64_t hash() const { return key.hash(); }
	bool operator==(const Key& otherKey) const { return this->key == otherKey; }
	bool operator==(const KeyValue& other) const { return this->key == other.key; }
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
