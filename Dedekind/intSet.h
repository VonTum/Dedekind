#pragma once

#include "iteratorEnd.h"
#include "iteratorFactory.h"

template<typename IntType, typename UnderlyingIntType>
class IntSetFilteredTrueIterator;
template<typename IntType, typename UnderlyingIntType>
class IntSetFilteredFalseIterator;

template<typename IntType, typename UnderlyingIntType = size_t>
class int_set {
	unsigned char* data;
	UnderlyingIntType bufSize;

	static constexpr unsigned char TRU = 255;
	static constexpr unsigned char FLS = 0;

	UnderlyingIntType getIndex(IntType v) const {
		UnderlyingIntType result = static_cast<UnderlyingIntType>(v);
		assert(result < bufSize);
		return result;
	}
public:
	int_set() : data(nullptr), bufSize(0) {}
	int_set(UnderlyingIntType bufSize) : data(new unsigned char[bufSize]), bufSize(bufSize) { for(UnderlyingIntType i = 0; i < bufSize; i++) data[i] = FLS; }
	template<typename Collection>
	int_set(UnderlyingIntType bufSize, const Collection& initialData) : data(new unsigned char[bufSize]), bufSize(bufSize) {
		for(UnderlyingIntType i = 0; i < bufSize; i++) {
			data[i] = FLS;
		}
		for(IntType v : initialData) {
			data[getIndex(v)] = TRU;
		}
	}
	int_set(int_set<IntType, UnderlyingIntType>&& other) : data(other.data), bufSize(other.bufSize) {
		other.data = nullptr;
		other.bufSize = 0;
	}
	int_set<IntType, UnderlyingIntType>& operator=(int_set<IntType, UnderlyingIntType>&& other) {
		std::swap(this->data, other.data);
		std::swap(this->bufSize, other.bufSize);
		return *this;
	}
	int_set(const int_set<IntType, UnderlyingIntType>& other) : data(new unsigned char[other.bufSize]), bufSize(other.bufSize) {
		for(UnderlyingIntType i = 0; i < bufSize; i++) {
			this->data[i] = other.data[i];
		}
	}
	int_set<IntType, UnderlyingIntType>& operator=(const int_set<IntType, UnderlyingIntType>& other) {
		delete[] this->data;
		this->data = new unsigned char[other.bufSize];
		this->bufSize = other.bufSize;
		for(UnderlyingIntType i = 0; i < other.bufSize; i++) {
			this->data[i] = other.data[i];
		}
		return *this;
	}

	~int_set() { delete[] data; }

	bool contains(IntType item) const {
		return data[getIndex(item)] == TRU;
	}

	bool containsFree(IntType item) const {
		UnderlyingIntType index = static_cast<UnderlyingIntType>(item);
		if(index < bufSize) {
			return data[index] == TRU;
		} else {
			return false;
		}
	}

	void add(IntType item) {
		data[getIndex(item)] = TRU;
	}

	void remove(IntType item) {
		data[getIndex(item)] = FLS;
	}

	template<typename Collection>
	bool containsAll(const Collection& col) const {
		for(IntType t : col) {
			if(data[getIndex(t)] == FLS) {
				return false;
			}
		}
		return true;
	}

	const unsigned char* getData() const {
		return data;
	}

	IntSetFilteredTrueIterator<IntType, UnderlyingIntType> begin() const {
		return IntSetFilteredTrueIterator<IntType, UnderlyingIntType>(data, bufSize);
	}
	IteratorEnd end() { return IteratorEnd(); }

	IteratorFactory<IntSetFilteredFalseIterator<IntType, UnderlyingIntType>> iterInverse() const {
		return IteratorFactory<IntSetFilteredFalseIterator<IntType, UnderlyingIntType>>{
			IntSetFilteredFalseIterator<IntType, UnderlyingIntType>(data, bufSize)
		};
	}
};

template<typename IntType, typename UnderlyingIntType>
class IntSetFilteredTrueIterator {
	unsigned char* data;
	UnderlyingIntType bufSize;
	UnderlyingIntType curIndex;
public:
	IntSetFilteredTrueIterator(unsigned char* data, UnderlyingIntType bufSize) : data(data), bufSize(bufSize), curIndex(0) {
		while(!data[curIndex] && curIndex < bufSize) {
			curIndex++;
		}
	}
	IntSetFilteredTrueIterator& operator++() {
		do {
			curIndex++;
		} while(!data[curIndex] && curIndex < bufSize);
		return *this;
	}
	bool operator==(IteratorEnd) const { return bufSize == curIndex; }
	bool operator!=(IteratorEnd) const { return bufSize != curIndex; }
	IntType operator*() const { return IntType{curIndex}; }
};

template<typename IntType, typename UnderlyingIntType>
class IntSetFilteredFalseIterator {
	unsigned char* data;
	UnderlyingIntType bufSize;
	UnderlyingIntType curIndex;
public:
	IntSetFilteredFalseIterator(unsigned char* data, UnderlyingIntType bufSize) : data(data), bufSize(bufSize), curIndex(0) {
		while(data[curIndex] && curIndex < bufSize) {
			curIndex++;
		}
	}
	IntSetFilteredFalseIterator& operator++() {
		do {
			curIndex++;
		} while(data[curIndex] && curIndex < bufSize);
		return *this;
	}
	bool operator==(IteratorEnd) const { return bufSize == curIndex; }
	bool operator!=(IteratorEnd) const { return bufSize != curIndex; }
	IntType operator*() const { return IntType{curIndex}; }
};
