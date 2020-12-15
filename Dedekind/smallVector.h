#pragma once

#include <assert.h>
#include <vector>

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

	T& front() { assert(sz > 0); return buf[0]; }
	const T& front() const { assert(sz > 0); return buf[0]; }
	T& back() { assert(sz > 0); return buf[sz-1]; }
	const T& back() const { assert(sz > 0); return buf[sz - 1]; }

	size_t size() const { return sz; }
	void resize(size_t newSize) { assert(newSize <= MaxSize); this->sz = newSize; }

	void clear() { sz = 0; }

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
