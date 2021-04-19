#pragma once

#include "iteratorEnd.h"
#include "iteratorFactory.h"

template<typename IntType, typename UnderlyingIntType = size_t>
class int_set {
	UnderlyingIntType bufSize;
	uint64_t* data;

	UnderlyingIntType bufElements() const {
		return (bufSize + 63) / 64;
	}
	UnderlyingIntType getIndex(IntType v) const {
		UnderlyingIntType result = static_cast<UnderlyingIntType>(v);
		assert(result < bufSize);
		return result;
	}
public:
	void clear() {
		for(UnderlyingIntType i = 0; i < bufElements(); i++) {
			data[i] = 0;
		}
	}
	int_set() : data(nullptr), bufSize(0) {}
	int_set(UnderlyingIntType bufSize) : data(new uint64_t[(bufSize + 63) / 64]), bufSize(bufSize) {
		clear();
	}


	void add(IntType item) {
		UnderlyingIntType index = getIndex(item);
		data[index / 64] |= 1ULL << (index % 64);
	}

	void remove(IntType item) {
		UnderlyingIntType index = getIndex(item);
		data[index / 64] &= ~(1ULL << (index%64));
	}
	
	template<typename Collection>
	int_set(UnderlyingIntType bufSize, const Collection& initialData) : data(new uint64_t[(bufSize + 63) / 64]), bufSize(bufSize) {
		clear();
		for(IntType v : initialData) {
			add(v);
		}
	}
	int_set(int_set<IntType, UnderlyingIntType>&& other) noexcept : data(other.data), bufSize(other.bufSize) {
		other.data = nullptr;
		other.bufSize = 0;
	}
	int_set<IntType, UnderlyingIntType>& operator=(int_set<IntType, UnderlyingIntType>&& other) {
		std::swap(this->data, other.data);
		std::swap(this->bufSize, other.bufSize);
		return *this;
	}
	int_set(const int_set<IntType, UnderlyingIntType>& other) : data(new uint64_t[other.bufElements()]), bufSize(other.bufSize) {
		for(UnderlyingIntType i = 0; i < other.bufElements(); i++) {
			this->data[i] = other.data[i];
		}
	}
	int_set<IntType, UnderlyingIntType>& operator=(const int_set<IntType, UnderlyingIntType>& other) {
		delete[] this->data;
		this->data = new uint64_t[other.bufElements()];
		this->bufSize = other.bufSize;
		for(UnderlyingIntType i = 0; i < other.bufElements(); i++) {
			this->data[i] = other.data[i];
		}
		return *this;
	}

	~int_set() { delete[] data; }

	bool contains(IntType item) const {
		UnderlyingIntType index = getIndex(item);
		return (data[index / 64] & (1ULL << (index % 64))) != 0;
	}

	bool containsFree(IntType item) const {
		UnderlyingIntType index = static_cast<UnderlyingIntType>(item);
		if(index < bufSize) {
			return (data[index / 64] & (1ULL << (index % 64))) != 0;
		} else {
			return false;
		}
	}

	template<typename Collection>
	bool containsAll(const Collection& col) const {
		for(IntType t : col) {
			if(!contains(t)) {
				return false;
			}
		}
		return true;
	}

	const unsigned char* getData() const {
		return reinterpret_cast<unsigned char*>(data);
	}
};
