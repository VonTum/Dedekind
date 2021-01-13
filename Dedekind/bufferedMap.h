#pragma once

#include <atomic>

constexpr size_t hashMapPrimes[]{
	53,97,193,389,769,1543,3079,6151,12289,24593,
	49157,98317,196613,393241,786433,1572869,3145739,6291469,
	12582917,25165843,50331653,100663319,201326611,402653189,805306457,1610612741
};

template<typename Key, typename Value>
class BufferedMap {
public:
	struct KeyValue {
		Key key;
		Value value;
	};

	constexpr static unsigned int BAD_INDEX = ~static_cast<unsigned int>(0);

	std::atomic<unsigned int>* hashTable;
	KeyValue* keyValueBuffer;
	std::atomic<unsigned int>* nextNodeBuffer;
	size_t buckets;
	size_t itemCount;
private:

	// These are NOT locked, locking should be done by CALLER
	std::atomic<unsigned int>* getBucketFor(uint64_t hash) const {
		return hashTable + hash % buckets;
	}
	std::pair<KeyValue*, std::atomic<unsigned int>*> getNodeFor(const Key& lookingFor, uint64_t hash) const {
		std::atomic<unsigned int>* cur = getBucketFor(hash);
		unsigned int index = cur->load();
		for(; index != BAD_INDEX; index = nextNodeBuffer[index].load()) {
			KeyValue& item = keyValueBuffer[index];
			if(item.key == lookingFor) {
				return std::make_pair(&item, cur);
			}
			cur = &nextNodeBuffer[index];
		}
		return std::make_pair(nullptr, cur);
	}
	std::pair<KeyValue*, std::atomic<unsigned int>*> getNodeFor(const Key& lookingFor) const {
		return getNodeFor(lookingFor, lookingFor.hash());
	}

	// These are protected by bucketLock
	unsigned int getReadOnlyBucket(uint64_t hash) const {
		//shared_lock_guard lg(bucketLock);
		return hashTable[hash % buckets].load();
	}
	KeyValue* getReadOnlyNode(const Key& lookingFor, uint64_t hash) const {
		unsigned int curIndex = getReadOnlyBucket(hash);
		while(curIndex != BAD_INDEX) {
			KeyValue& item = keyValueBuffer[curIndex];
			if(item.key == lookingFor) {
				return &item;
			} else {
				curIndex = nextNodeBuffer[curIndex].load();
			}
		}
		return nullptr;
	}
	KeyValue* getReadOnlyNode(const Key& lookingFor) const {
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
				std::atomic<unsigned int>& bucket = hashTable[keyValueBuffer[curNode].key.hash() % buckets];
				unsigned int nextNode = nextNodeBuffer[curNode].load();
				unsigned int oldBucketNode = bucket.exchange(curNode);
				nextNodeBuffer[curNode].store(oldBucketNode);
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
			this->keyValueBuffer[i].~KeyValue();
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
		delete[] this->keyValueBuffer;
		this->keyValueBuffer = new KeyValue[newSize];
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
	BufferedMap(size_t capacity) : hashTable(new std::atomic<unsigned int>[getNumberOfBucketsForSize(capacity)]), nextNodeBuffer(new std::atomic<unsigned int>[capacity]), keyValueBuffer(new KeyValue[capacity]), buckets(getNumberOfBucketsForSize(capacity)), itemCount(0) {
		for(size_t i = 0; i < this->buckets; i++) {
			this->hashTable[i].store(BAD_INDEX);
		}
		for(size_t i = 0; i < this->itemCount; i++) {
			this->nextNodeBuffer[i].store(BAD_INDEX);
		}
	}
	BufferedMap() : hashTable(nullptr), nextNodeBuffer(nullptr), keyValueBuffer(nullptr), buckets(0), itemCount(0) {}

	BufferedMap(BufferedMap&& other) : hashTable(other.hashTable), nextNodeBuffer(other.nextNodeBuffer), keyValueBuffer(other.keyValueBuffer), buckets(other.buckets), itemCount(other.itemCount) {
		other.hashTable = nullptr;
		other.buckets = 0;
		other.nextNodeBuffer = nullptr;
		other.keyValueBuffer = nullptr;
		other.itemCount = 0;
	}
	BufferedMap& operator=(BufferedMap&& other) noexcept {
		std::swap(this->hashTable, other.hashTable);
		std::swap(this->buckets, other.buckets);
		std::swap(this->nextNodeBuffer, other.nextNodeBuffer);
		std::swap(this->keyValueBuffer, other.keyValueBuffer);
		std::swap(this->itemCount, other.itemCount);
		return *this;
	}

	~BufferedMap() {
		this->clear();
		delete[] hashTable;
		delete[] nextNodeBuffer;
		delete[] keyValueBuffer;
		buckets = 0;
		itemCount = 0;
	}

	KeyValue* find(const Key& key, uint64_t hash) {
		return getReadOnlyNode(key, hash);
	}
	KeyValue* find(const Key& key) {
		return this->find(key, key.hash());
	}
	const KeyValue* find(const Key& key, uint64_t hash) const {
		return getReadOnlyNode(key, hash);
	}
	const KeyValue* find(const Key& key) const {
		return this->find(key, key.hash());
	}

	Value& get(const Key& key) {
		KeyValue* foundNode = getReadOnlyNode(key);
		assert(foundNode != nullptr);
		return foundNode->value;
	}
	const Value& get(const Key& key) const {
		KeyValue* foundNode = getReadOnlyNode(key);
		assert(foundNode != nullptr);
		return foundNode->value;
	}
	KeyValue& getOrAdd(const Key& key, const Value& defaultForCreate, uint64_t hash) {
		std::pair<KeyValue*, std::atomic<unsigned int>*> foundNode = getNodeFor(key, hash);
		KeyValue* foundKeyValue = foundNode.first;
		std::atomic<unsigned int>& referer = *foundNode.second;
		if(foundKeyValue == nullptr) { // not found
			unsigned int newNodeIdx = this->itemCount;
			new(&keyValueBuffer[newNodeIdx]) KeyValue{key, defaultForCreate};
			nextNodeBuffer[newNodeIdx].store(BAD_INDEX);
			referer.store(newNodeIdx);
			this->itemCount++;
			return keyValueBuffer[newNodeIdx];
		} else {
			return *foundKeyValue;
		}
	}
	KeyValue& getOrAdd(const Key& key, const Value& defaultForCreate) {
		uint64_t hash = key.hash();
		return getOrAdd(key, defaultForCreate, hash);
	}

	KeyValue& add(const Key& key, const Value& value, uint64_t hash) {
		std::atomic<unsigned int>& bucket = *getBucketFor(hash);
		unsigned int newNodeIdx = this->itemCount;
		new(&keyValueBuffer[newNodeIdx]) KeyValue{key, value};
		unsigned int oldBucketValue = bucket.load();
		nextNodeBuffer[newNodeIdx].store(oldBucketValue);
		assert(oldBucketValue == -1 || oldBucketValue < newNodeIdx);
		bucket.store(newNodeIdx);
		this->itemCount++;
		return keyValueBuffer[newNodeIdx];
	}

	KeyValue& add(const Key& key, const Value& value) {
		return this->add(key, value, key.hash());
	}

	size_t size() const { return itemCount; }

	KeyValue* begin() { return keyValueBuffer; }
	const KeyValue* begin() const { return keyValueBuffer; }
	const KeyValue* end() const { return keyValueBuffer + itemCount; }
	KeyValue* end() { return keyValueBuffer + itemCount; }
};

template<typename T>
class BufferedSet {
	struct EmptyValue {};
	BufferedMap<T, EmptyValue> underlyingMap;

public:
	BufferedSet() : underlyingMap() {}
	BufferedSet(size_t size) : underlyingMap(size) {}

	T& add(const T& item) {
		return underlyingMap.getOrAdd(item, EmptyValue{}).key;
	}
	bool contains(const T& item) const {
		return underlyingMap.find(item) != nullptr;
	}
	void clear() {
		underlyingMap.clear();
	}
	void resizeWithClear(size_t newSize) {
		underlyingMap.resizeWithClear(newSize);
	}
	size_t size() const {
		return underlyingMap.size();
	}

	/*struct SetIter {
		BufferedMap<T, EmptyValue>::unsigned int cur;

		SetIter& operator++() {
			cur++;
			return *this;
		}

		bool operator==(const SetIter& other) const {
			return cur == other.cur;
		}
		bool operator!=(const SetIter& other) const {
			return cur != other.cur;
		}

		T& operator*() {
			return cur->item.key;
		}
	};*/
	using SetNode = typename BufferedMap<T, EmptyValue>::KeyValue;
	SetNode* begin() { return underlyingMap.keyValueBuffer; }
	SetNode* end() { return underlyingMap.keyValueBuffer + underlyingMap.itemCount; }
};
