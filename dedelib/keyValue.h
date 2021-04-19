#pragma once



template<typename Key, typename Value>
struct KeyValue {
	Key key;
	Value value;

	uint64_t hash() const { return key.hash(); }
	bool operator==(const Key& otherKey) const { return this->key == otherKey; }
	bool operator==(const KeyValue& other) const { return this->key == other.key; }
};


constexpr size_t hashMapPrimes[]{
	53,97,193,389,769,1543,3079,6151,12289,24593,
	49157,98317,196613,393241,786433,1572869,3145739,6291469,
	12582917,25165843,50331653,100663319,201326611,402653189,805306457,1610612741
};

inline size_t getNumberOfBucketsForSize(size_t size) {
	size_t lastBp;
	for(const size_t& bp : hashMapPrimes) {
		if(bp > size) {
			return bp;
		}
		lastBp = bp;
	}
	return lastBp;
}


