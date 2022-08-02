#pragma once

#include <stddef.h>

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

void allocSocketBuffers(size_t bufSize, void* socketBuffers[2]);
void allocCoreComplexBuffers(size_t bufSize, void* buffers[8]);
void duplicateNUMAData(const void* from, void** buffers, size_t numBuffers, size_t bufferSize);

template<typename T>
T* numa_alloc_T(size_t bufSize, size_t numaNode) {
	return (T*) numa_alloc_onnode(bufSize * sizeof(T), numaNode);
}

