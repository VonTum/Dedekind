#pragma once

#include <stddef.h>
#include <cassert>

//#ifndef NO_NUMA
#include <numa.h>
/*#else
#include "aligned_alloc.h"
inline void numa_free(void* ptr) {
	aligned_free(ptr);
}
inline void* numa_alloc_onnode(size_t size) {
	aligned_malloc(size, 4096); // numa alloc alignment
}
#endif*/

void* allocInterleaved(size_t bufSize, const char* nodeString);
void allocSocketBuffers(size_t bufSize, void* socketBuffers[2]);
void allocCoreComplexBuffers(size_t bufSize, void* buffers[8]);
void duplicateNUMAData(const void* from, void** buffers, size_t numBuffers, size_t bufferSize);

template<typename T>
T* numa_alloc_T(size_t bufSize, size_t numaNode) {
	return (T*) numa_alloc_onnode(bufSize * sizeof(T), numaNode);
}

template<typename T>
T* numa_alloc_interleaved_T(size_t bufSize, const char* nodeString) {
	return (T*) allocInterleaved(bufSize * sizeof(T), nodeString);
}

template<typename T>
void numa_free_T(T* mem, size_t bufSize) {
	numa_free((void*) mem, bufSize * sizeof(T));
}

template<typename T>
struct NUMAArray {
	T* data;
	size_t size;
	
	NUMAArray() : data(nullptr) {}
	NUMAArray(T* data, size_t size) : data(data), size(size) {}
	~NUMAArray() {
		numa_free((void*) this->data, sizeof(T) * size);
	}
	NUMAArray& operator=(NUMAArray&& other) noexcept {
		T* d = this->data;
		this->data = other.data;
		other.data = d;
		size_t s = this->size;
		this->size = other.size;
		other.size = s;
	}
	NUMAArray(const NUMAArray& other) = delete;
	NUMAArray& operator=(const NUMAArray& other) = delete;
	
	T& operator[](size_t idx) {assert(idx < size); return data[idx];}
};
