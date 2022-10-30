#include "numaMem.h"

#include <string.h>
#include <xmmintrin.h>
#include <emmintrin.h>
#include <immintrin.h>

#ifndef USE_NUMA
#include "aligned_alloc.h"
void numa_free(void* ptr, size_t size) {
	aligned_free(ptr);
}
void* numa_alloc_onnode(size_t size, int numaNode) {
	return aligned_malloc(size, 4096); // numa alloc alignment
}
void* numa_alloc_interleaved(size_t size) {
	return aligned_malloc(size, 4096); // numa alloc alignment
}
#endif

void* allocInterleaved(size_t bufSize, const char* nodeString) {
#ifdef USE_NUMA
	struct bitmask* nodeMask = numa_parse_nodestring(nodeString);
	void* result = numa_alloc_interleaved_subset(bufSize, nodeMask);
	numa_free_nodemask(nodeMask);
	return result;
#else
	return aligned_malloc(bufSize, 4096); // numa alloc alignment
#endif
}

void* numa_alloc_onsocket(size_t size, unsigned int socket) {
	assert(socket < 2);
	const char* sockets[]{"0-3", "4-7"};
	return allocInterleaved(size, sockets[socket]);
}

void allocSocketBuffers(size_t bufSize, void* socketBuffers[2]) {
	socketBuffers[0] = allocInterleaved(bufSize, "0-3");
	socketBuffers[1] = allocInterleaved(bufSize, "4-7");
}

void allocNumaNodeBuffers(size_t bufSize, void* buffers[8]) {
	for(int nn = 0; nn < 8; nn++) {
		buffers[nn] = numa_alloc_onnode(bufSize, nn);
	}
}

// May overshoot a little but shouldn't be a problem
void duplicateNUMAData(const void* from, void** buffers, size_t numBuffers, size_t bufferSize) {
    constexpr size_t BLOCKS_PER_ITER = 8;
    constexpr size_t ALIGN = BLOCKS_PER_ITER * 32;
	size_t numBlocks = (bufferSize + ALIGN - 1) / ALIGN;
	const __m256i* originBuf = reinterpret_cast<const __m256i*>(from);
	for(size_t blockI = 0; blockI < numBlocks; blockI++) {
		__m256i blocks[BLOCKS_PER_ITER];
        for(size_t i = 0; i < BLOCKS_PER_ITER; i++) {
            blocks[i] = _mm256_stream_load_si256(originBuf + blockI * BLOCKS_PER_ITER + i);
        }

		for(size_t bufI = 0; bufI < numBuffers; bufI++) {
			__m256i* targetBuf = reinterpret_cast<__m256i*>(buffers[bufI]);
            for(size_t i = 0; i < BLOCKS_PER_ITER; i++) {
			    _mm256_stream_si256(targetBuf + blockI * BLOCKS_PER_ITER + i, blocks[i]);
            }
		}
	}
}
