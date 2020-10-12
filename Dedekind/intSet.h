#pragma once

template<typename IntType, typename UnderlyingIntType = size_t>
class int_set {
	bool* data;
	UnderlyingIntType bufSize;

	UnderlyingIntType getIndex(IntType v) const {
		UnderlyingIntType result = static_cast<UnderlyingIntType>(v);
		assert(result < bufSize);
		return result;
	}
public:
	int_set() : data(nullptr), bufSize(0) {}
	int_set(UnderlyingIntType bufSize) : data(new bool[bufSize]), bufSize(bufSize) { for(UnderlyingIntType i = 0; i < bufSize; i++) data[i] = false; }
	template<template<typename> typename Collection>
	int_set(UnderlyingIntType bufSize, const Collection<IntType>& initialData) : data(new bool[bufSize]), bufSize(bufSize) {
		for(UnderlyingIntType i = 0; i < bufSize; i++) data[i] = false;
		for(IntType v : initialData) {
			data[getIndex(v)] = true;
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
	int_set(const int_set<IntType, UnderlyingIntType>& other) : data(new bool[other.bufSize]), bufSize(other.bufSize) {
		for(UnderlyingIntType i = 0; i < bufSize; i++) {
			this->data[i] = other.data[i];
		}
	}
	int_set<IntType, UnderlyingIntType>& operator=(const int_set<IntType, UnderlyingIntType>& other) {
		delete[] this->data;
		this->data = new bool[other.bufSize];
		this->bufSize = other.bufSize;
		for(UnderlyingIntType i = 0; i < other.bufSize; i++) {
			this->data[i] = other.data[i];
		}
		return *this;
	}

	~int_set() { delete[] data; }

	bool contains(IntType item) const {
		return data[getIndex(item)];
	}

	void add(IntType item) {
		data[getIndex(item)] = true;
	}

	void remove(IntType item) {
		data[getIndex(item)] = false;
	}

	template<template<typename> typename Collection>
	bool containsAll(const Collection<IntType>& col) const {
		for(IntType t : col) {
			if(data[getIndex(t)] == false) {
				return false;
			}
		}
		return true;
	}
};

