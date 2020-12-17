#pragma once

#include <assert.h>
#include <vector>
#include <initializer_list>

template<typename T, size_t MaxSize>
class SmallVector {
	size_t sz;
	T buf[MaxSize];
public:
	SmallVector() : sz(0) {}
	SmallVector(size_t initialSize) : sz(initialSize) { assert(initialSize <= MaxSize); }
	SmallVector(size_t initialSize, const T& defaultValue) : sz(initialSize) {
		assert(initialSize <= MaxSize);
		for(size_t i = 0; i < initialSize; i++) {
			buf[i] = defaultValue;
		}
	}
	SmallVector(std::initializer_list<T> initialValues) : sz(initialValues.size()) {
		assert(initialValues.size() <= MaxSize);
		size_t i = 0;
		for(const T& item : initialValues) {
			buf[i++] = item;
		}
	}

	SmallVector(const SmallVector<T, MaxSize>& other) : sz(other.sz) {
		for(size_t i = 0; i < other.sz; i++) {
			new(buf + i) T(other.buf[i]);
		}
	}
	SmallVector& operator=(const SmallVector<T, MaxSize>& other) {
		for(size_t i = 0; i < this->sz; i++) {
			buf[i].~T();
		}
		this->sz = other.sz;
		for(size_t i = 0; i < other.sz; i++) {
			new(buf + i) T(other.buf[i]);
		}
		return *this;
	}
	SmallVector(SmallVector<T, MaxSize>&& other) noexcept : sz(other.sz) {
		for(size_t i = 0; i < other.sz; i++) {
			new(buf + i) T(std::move(other.buf[i]));
		}
		other.sz = 0;
	}
	SmallVector& operator=(SmallVector<T, MaxSize>&& other) noexcept {
		for(size_t i = 0; i < this->sz; i++) {
			buf[i].~T();
		}
		this->sz = other.sz;
		for(size_t i = 0; i < other.sz; i++) {
			new(buf + i) T(std::move(other.buf[i]));
		}
		other.sz = 0;
		return *this;
	}

	T& push_back(const T& item) {
		assert(sz + 1 <= MaxSize);
		buf[sz] = item;
		return buf[sz++];
	}
	T& push_back(T&& item) {
		assert(sz + 1 <= MaxSize);
		buf[sz] = std::move(item);
		return buf[sz++];
	}
	template<typename... Args>
	T& emplace_back(Args&&... args) {
		assert(sz + 1 <= MaxSize);
		new(&buf + sz) T(args...);
		return buf[sz++];
	}

	bool contains(const T& item) const {
		for(size_t i = 0; i < sz; i++) {
			if(buf[i] == item) {
				return true;
			}
		}
		return false;
	}

	void reserve(size_t size) {
		assert(size <= MaxSize);
	}

	T& front() { assert(sz > 0); return buf[0]; }
	const T& front() const { assert(sz > 0); return buf[0]; }
	T& back() { assert(sz > 0); return buf[sz - 1]; }
	const T& back() const { assert(sz > 0); return buf[sz - 1]; }

	size_t size() const { return sz; }
	void resize(size_t newSize) { 
		assert(newSize <= MaxSize); 
		if(newSize < this->sz) {
			for(size_t i = newSize; i < this->sz; i++) {
				this->buf[i].~T();
			}
		} else {
			for(size_t i = this->sz; i < newSize; i++) {
				new(this->buf + i) T();
			}
		}
		this->sz = newSize;
	}

	void clear() {
		for(size_t i = 0; i < sz; i++) {
			buf[i].~T();
		}
		sz = 0;
	}

	T& operator[](size_t index) { assert(index < sz); return buf[index]; }
	const T& operator[](size_t index) const { assert(index < sz); return buf[index]; }

	T* begin() { return buf; }
	const T* begin() const { return buf; }
	T* end() { return buf + sz; }
	const T* end() const { return buf + sz; }
};

template<typename T1, size_t MaxSize1, typename T2, size_t MaxSize2>
bool operator==(const SmallVector<T1, MaxSize1>& v1, const SmallVector<T2, MaxSize2>& v2) {
	if(v1.size() != v2.size()) return false;

	for(size_t i = 0; i < v1.size(); i++) {
		if(!(v1[i] == v2[i])) return false;
	}
	return true;
}

template<typename T1, size_t MaxSize1, typename T2, size_t MaxSize2>
bool operator!=(const SmallVector<T1, MaxSize1>& v1, const SmallVector<T2, MaxSize2>& v2) {
	return !(v1 == v2);
}
