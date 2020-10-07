#pragma once

#include <cstdlib>
#include <cassert>
#include <vector>

template<typename T>
class set {
	T* data;
	size_t n;
	size_t capacity;

	static T* allocT(size_t size) {
		return static_cast<T*>(malloc(sizeof(T) * size));
	}
	static void freeT(T* buf, size_t size) {
		for(size_t i = 0; i < size; i++) {
			buf[i].~T();
		}
		free(buf);
	}
	
	void expandIfNeeded(size_t newSize) {
		if(newSize > capacity) {
			size_t newCapacity = capacity * 2 + 1;
			while(newSize > newCapacity) newCapacity *= 2;

			T* newData = allocT(newCapacity);

			for(size_t i = 0; i < n; i++) {
				new(newData + i) T(std::move(data[i]));
			}
			free(data);
			this->data = newData;
			this->capacity = newCapacity;
		}
	}
public:
	set() : data(nullptr), n(0), capacity(0) {}
	set(size_t size) : data(allocT(size)), n(size), capacity(capacity) {
		for(T& item : *this) {
			new(&item) T();
		}
	}
	set(const std::vector<T>& vec) : data(allocT(vec.size())), n(vec.size()), capacity(vec.size()) {
		for(size_t i = 0; i < n; i++) {
			new(data + i) T(vec[i]);
		}
	}
	~set() {
		freeT(data, n);
		n = 0;
		capacity = 0;
		data = nullptr;
	}

	set(const set<T>& other) : data(allocT(other.n)), n(other.n), capacity(other.n) {
		for(size_t i = 0; i < other.n; i++) {
			data[i] = other[i];
		}
	}
	set<T>& operator=(const set<T>& other) {
		freeT(data, n);
		data = allocT(other.n);
		n = other.n;
		capacity = other.n;
		for(size_t i = 0; i < other.n; i++) {
			data[i] = other[i];
		}
		return *this;
	}
	set(set<T>&& other) noexcept : data(other.data), n(other.n), capacity(other.capacity) {
		other.data = nullptr;
		other.n = 0;
		other.capacity = 0;
	}
	set<T>& operator=(set<T>&& other) noexcept {
		std::swap(data, other.data);
		std::swap(n, other.n);
		std::swap(capacity, other.capacity);
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
	void remove(size_t index) {
		assert(index < n);
		n--;
		data[index] = std::move(data[n]);
		data[n].~T();
	}
	void remove(T* item) {
		assert(item >= data && item < data+n);
		n--;
		*item = std::move(data[n]);
		data[n].~T();
	}
	set<T> withIndexRemoved(size_t index) const {
		set<T> result = *this;
		result.remove(index);
		return result;
	}
	size_t size() const { return n; }

	T* begin() { return data; }
	T* end() { return data + n; }
	const T* begin() const { return data; }
	const T* end() const { return data + n; }
};
