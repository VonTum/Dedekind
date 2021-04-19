#pragma once

#include <assert.h>
#include <utility>

template<typename T>
class HeapArray {
	T* data;
	size_t bufSize;
public:

	HeapArray() : data(nullptr), bufSize(0) {}
	HeapArray(size_t bufSize) : data(new T[bufSize]), bufSize(bufSize) {}

	~HeapArray() {
		delete[] data;
		bufSize = 0;
	}

	HeapArray(HeapArray&& other) noexcept : data(other.data), bufSize(other.bufSize) {
		other.data = nullptr;
		other.bufSize = 0;
	}

	HeapArray& operator=(HeapArray&& other) noexcept {
		std::swap(this->data, other.data);
		std::swap(this->bufSize, other.bufSize);
		return *this;
	}

	HeapArray(const HeapArray& other) : data(new T[other.bufSize]), bufSize(other.bufSize) {
		for(size_t i = 0; i < other.bufSize; i++) {
			this->data[i] = other.data[i];
		}
	}

	HeapArray& operator=(const HeapArray& other) {
		delete[] this->data;
		this->data = new T[other.bufSize];
		this->bufSize = other.bufSize;
		for(size_t i = 0; i < other.bufSize; i++) {
			this->data[i] = other.data[i];
		}
		return *this;
	}

	size_t size() const { return bufSize; }
	T* ptr() { return data; }
	const T* ptr() const { return data; }

	T& operator[](size_t index) { assert(index >= 0 && index < bufSize); return data[index]; }
	const T& operator[](size_t index) const { assert(index >= 0 && index < bufSize); return data[index]; }

	T* begin() { return data; }
	const T* begin() const { return data; }
	T* end() { return data + bufSize; }
	const T* end() const { return data + bufSize; }
};
