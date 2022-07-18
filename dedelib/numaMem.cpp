#include "numaMem.h"

#include <numa.h>
#include <string.h>
#include <xmmintrin.h>
#include <emmintrin.h>
#include <immintrin.h>

void allocSocketBuffers(size_t bufSize, void* socketBuffers[2]) {
	struct bitmask* nodeMask0 = numa_parse_nodestring("0-3");
	socketBuffers[0] = numa_alloc_interleaved_subset(bufSize, nodeMask0);
	numa_free_nodemask(nodeMask0);

	struct bitmask* nodeMask1 = numa_parse_nodestring("4-7");
	socketBuffers[1] = numa_alloc_interleaved_subset(bufSize, nodeMask1);
	numa_free_nodemask(nodeMask1);
}

void allocCoreComplexBuffers(size_t bufSize, void* buffers[8]) {
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
