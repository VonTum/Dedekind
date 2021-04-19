#pragma once


#pragma once

#include <cstdlib>
#include <cassert>
#include <vector>
#include <initializer_list>

#include "aligned_alloc.h"

template<typename T, size_t Align = alignof(T)>
class aligned_set {
	T* data;
	size_t n;
	size_t capacity;

	static T* allocT(size_t size) {
		return static_cast<T*>(aligned_malloc(sizeof(T) * size, Align));
	}
	static void freeT(T* buf, size_t size) {
		for(size_t i = 0; i < size; i++) {
			buf[i].~T();
		}
		aligned_free(buf);
	}

	static size_t nextAligned(size_t value) {
		size_t leftOver = value % (Align / sizeof(T));
		if(leftOver == 0) {
			return value;
		} else {
			return value + (Align / sizeof(T)) - leftOver;
		}
	}

	void expandIfNeeded(size_t newSize) {
		if(newSize > capacity) {
			size_t newCapacity = nextAligned(capacity + 1);

			T* newData = allocT(newCapacity);

			for(size_t i = 0; i < n; i++) {
				new(newData + i) T(std::move(data[i]));
			}
			freeT(this->data, this->n);
			this->data = newData;
			this->capacity = newCapacity;
		}
	}

	aligned_set(size_t size, size_t capacity) : data(allocT(capacity)), n(size), capacity(capacity) {}
public:
	aligned_set() : data(nullptr), n(0), capacity(0) {}
	aligned_set(size_t size) : aligned_set(size, nextAligned(size)) {
		for(T& item : *this) {
			new(&item) T();
		}
	}
	aligned_set(size_t size, const T& val) : aligned_set(size) {
		for(size_t i = 0; i < size; i++) {
			new(data + i) T(val);
		}
	}
	aligned_set(std::initializer_list<T> lst) : aligned_set(lst.size()) {
		T* cur = data;
		for(const T& item : lst) {
			new(cur) T(item);
			cur++;
		}
	}
	aligned_set(const std::vector<T>& vec) : aligned_set(vec.size()) {
		for(size_t i = 0; i < n; i++) {
			new(data + i) T(vec[i]);
		}
	}
	~aligned_set() {
		freeT(data, n);
		n = 0;
		capacity = 0;
		data = nullptr;
	}

	aligned_set(const aligned_set& other) : aligned_set(other.n) {
		for(size_t i = 0; i < nextAligned(other.n); i++) {
			data[i] = other.data[i];
		}
	}
	aligned_set& operator=(const aligned_set& other) {
		freeT(data, n);
		data = allocT(other.n);
		n = other.n;
		capacity = other.n;
		for(size_t i = 0; i < nextAligned(other.n); i++) {
			data[i] = other.data[i];
		}
		return *this;
	}
	aligned_set(aligned_set&& other) noexcept : data(other.data), n(other.n), capacity(other.capacity) {
		other.data = nullptr;
		other.n = 0;
		other.capacity = 0;
	}
	aligned_set& operator=(aligned_set&& other) noexcept {
		std::swap(data, other.data);
		std::swap(n, other.n);
		std::swap(capacity, other.capacity);
		return *this;
	}

	const T& operator[](size_t index) const { assert(index < n); return data[index]; }
	T& operator[](size_t index) { assert(index < n); return data[index]; }

	void push_back(T&& newItem) {
		expandIfNeeded(n + 1);
		new(data + n) T(std::move(newItem));
		n++;
	}
	void push_back(const T& newItem) {
		expandIfNeeded(n + 1);
		new(data + n) T(newItem);
		n++;
	}
	void pop_back() {
		n--;
		data[n].~T();
	}
	void remove(size_t index) {
		assert(index < n);
		n--;
		data[index] = std::move(data[n]);
		data[n] = T();
	}
	void remove(T* item) {
		assert(item >= data && item < data + n);
		n--;
		*item = std::move(data[n]);
		data[n] = T();
	}
	aligned_set withIndexRemoved(size_t index) const {
		aligned_set result = *this;
		result.remove(index);
		return result;
	}

	bool contains(const T& item) const {
		for(size_t i = 0; i < n; i++) {
			if(data[i] == item) {
				return true;
			}
		}
		return false;
	}

	size_t size() const { return n; }
	void reserve(size_t newSize) { expandIfNeeded(newSize); }
	T* getData() { return data; }
	const T* getData() const { return data; }

	void fixFinalBlock() {
		size_t endOfGatherableData = nextAligned(this->n);
		if(this->n != endOfGatherableData) {
			T valueToCopy = this->data[this->n - 1];
			for(size_t i = this->n; i < endOfGatherableData; i++) {
				this->data[i] = valueToCopy;
			}
		}
	}

	T* begin() { return data; }
	T* end() { return data + n; }
	const T* begin() const { return data; }
	const T* end() const { return data + n; }
};

template<typename T, size_t Align>
bool operator==(const aligned_set<T, Align>& a, aligned_set<T, Align> b) {
	assert(a.size() == b.size());

	for(const T& itemInA : a) {
		const T* bEnd = b.end();
		for(T* bIter = b.begin(); bIter != bEnd; ++bIter) {
			if(itemInA == *bIter) {
				b.remove(bIter);
				goto found;
			}
		}
		return false;
		found:;
	}
	return true;
}

template<typename T, size_t Align>
bool operator!=(const aligned_set<T, Align>& a, aligned_set<T, Align> b) {
	return !(a == b);
}
