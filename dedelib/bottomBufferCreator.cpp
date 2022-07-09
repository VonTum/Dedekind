#include "bottomBufferCreator.h"

#include "crossPlatformIntrinsics.h"

#include "knownData.h"

#include <xmmintrin.h>
#include <emmintrin.h>
#include <immintrin.h>

#include <cstring>
#include <vector>
#include <algorithm>
#include <atomic>
#include <thread>
#include <chrono>

#include "flatBufferManagement.h"
#include "fileNames.h"

#include "aligned_alloc.h"

#include "threadUtils.h"

typedef uint8_t swapper_block;
constexpr int BUFFERS_PER_BATCH = sizeof(swapper_block) * 8;

#define FPGA_BLOCK_SIZE 32
#define FPGA_BLOCK_ALIGN (FPGA_BLOCK_SIZE * sizeof(uint32_t))

/*
 * Swapper code
 */

// Returns the starting layer aka the highest layer
static int initializeSwapperRun(
	unsigned int Variables,
	swapper_block* __restrict swapper,
	uint32_t* __restrict * __restrict resultBuffers,
	const JobTopInfo* __restrict tops,
	int* topLayers,
	int numberOfTops
) {
	std::memset(swapper, 0, sizeof(swapper_block) * getMaxLayerSize(Variables));

	int highestLayer = 0;
	for(int i = 0; i < numberOfTops; i++) {
		uint32_t curTop = tops[i].top;
		uint32_t topDual = tops[i].topDual;
		int layer = getFlatLayerOfIndex(Variables, curTop);
		if(layer > highestLayer) highestLayer = layer;
		topLayers[i] = layer;

		*resultBuffers[i]++ = curTop | uint32_t(0x80000000); // Mark the incoming top so that the processor can detect it
		*resultBuffers[i]++ = curTop | uint32_t(0x80000000); // Double because the FPGA is double-pumped

#ifdef PCOEFF_DEDUPLICATE
		*resultBuffers[i]++ = topDual; // Add dual in fixed location. That way it doesn't need to be computed by resultsCollection
		if(curTop < topDual)
#endif
			*resultBuffers[i]++ = curTop;
	}


	return highestLayer;
}

static void initializeSwapperTops(
	unsigned int Variables,
	swapper_block* __restrict swapper,
	int currentLayer,
	const JobTopInfo* __restrict tops,
	const int* topLayers,
	int numberOfTops
) {
	for(int topI = 0; topI < numberOfTops; topI++) {
		if(topLayers[topI] == currentLayer) {
			uint32_t topIdxInLayer = tops[topI].top - flatNodeLayerOffsets[Variables][currentLayer];
			swapper[topIdxInLayer] |= swapper_block(1) << topI;
		}
	}
}

static void optimizeBlockForFPGA(uint32_t* buf) {
	assert(reinterpret_cast<uintptr_t>(buf) % FPGA_BLOCK_ALIGN == 0);
	if(__builtin_expect(((buf[0] & 0x80000000) == 0), 1)) {
		uint32_t tmpBuf[FPGA_BLOCK_SIZE];
		for(int i = 0; i < FPGA_BLOCK_SIZE; i++) {
			tmpBuf[i] = buf[i];
		}
		for(int i = 0; i < FPGA_BLOCK_SIZE/2; i++) {
			buf[2*i] = tmpBuf[i];
			buf[2*i+1] = tmpBuf[i+FPGA_BLOCK_SIZE/2];
		}
	}
}

static void addValueToBlockFPGA(uint32_t*__restrict& buf, uint32_t newValue) {
	*buf = newValue;
	//_mm_stream_si32(reinterpret_cast<int*>(buf), static_cast<int>(newValue)); // TODO Compare performance
	buf++;
	if(__builtin_expect(reinterpret_cast<uintptr_t>(buf) % FPGA_BLOCK_ALIGN == 0, 0)) {
		optimizeBlockForFPGA(buf - FPGA_BLOCK_SIZE);
	}
}

// Computes 64 result buffers at a time
static swapper_block computeNextLayer(
	const uint32_t* __restrict links, // Links index from the previous layer to this layer. Last element of a link streak will have a 1 in the 31 bit position. 
	const JobTopInfo* tops,
	const swapper_block* __restrict swapperIn,
	swapper_block* __restrict swapperOut,
	uint32_t resultIndexOffset,
	uint32_t numberOfLinksToLayer,
	uint32_t* __restrict * __restrict resultBuffers,
	swapper_block activeMask // Has a 1 for active buffers
#ifndef NDEBUG
	,uint32_t fromLayerSize,
	uint32_t toLayerSize
#endif
) {
	swapper_block finishedConnections = ~swapper_block(0);

	swapper_block currentConnectionsIn = 0;
	uint32_t curNodeI = 0;
	for(uint32_t linkI = 0; linkI < numberOfLinksToLayer; linkI++) {
		uint32_t curLink = links[linkI];
		uint32_t fromIdx = curLink & uint32_t(0x7FFFFFFF);
		assert(fromIdx < fromLayerSize);
		currentConnectionsIn |= swapperIn[fromIdx];
		if((curLink & uint32_t(0x80000000)) != 0) {
			currentConnectionsIn &= activeMask; // only check currently active ones
			finishedConnections &= currentConnectionsIn;
			assert(curNodeI < toLayerSize);
			swapperOut[curNodeI] = currentConnectionsIn;
			
			uint32_t thisValue = resultIndexOffset + curNodeI;

			curNodeI++;

			while(currentConnectionsIn != 0) {
				static_assert(BUFFERS_PER_BATCH <= 8);
				int idx = ctz8(currentConnectionsIn);
				currentConnectionsIn &= ~(swapper_block(1) << idx);

#ifdef PCOEFF_DEDUPLICATE
				if(thisValue < tops[idx].topDual)
#endif
					addValueToBlockFPGA(resultBuffers[idx], thisValue);
			}
		}

		// Prefetch a bit in advance
		/*uint32_t prefetchLinkI = linkI + 512; // PREFETCH OFFSET
		if(prefetchLinkI < numberOfLinksToLayer) {
			uint32_t prefetchLink = links[prefetchLinkI];
			uint32_t prefetchIdx = prefetchLink & uint32_t(0x7FFFFFFF);
			_mm_prefetch(swapperIn + prefetchIdx, _MM_HINT_T0);
		}*/
	}

	assert(curNodeI == toLayerSize);

	return finishedConnections;
}

static void finalizeBuffer(uint32_t* __restrict & resultBuffer, uint32_t fillUpTo) {
	uint32_t curI = 0;
	if(fillUpTo >= FPGA_BLOCK_SIZE) {
		uint32_t* __restrict resultBufPtr = resultBuffer;
		uintptr_t alignOffset = (reinterpret_cast<uintptr_t>(resultBufPtr) / sizeof(uint32_t)) % FPGA_BLOCK_SIZE;
		if(alignOffset != 0) {
			uint32_t alignmentElementCount = FPGA_BLOCK_SIZE - alignOffset;
			fillUpTo = fillUpTo - alignmentElementCount;
			for(uint32_t i = 0; i < alignmentElementCount; i++) {
				*resultBuffer++ = fillUpTo + i;
			}
			optimizeBlockForFPGA(resultBuffer - FPGA_BLOCK_SIZE);
		}
	}
	// Generate aligned blocks efficiently
	uint32_t numBlocks = fillUpTo / FPGA_BLOCK_SIZE;
	if(numBlocks >= 1) {
		static_assert(FPGA_BLOCK_SIZE == 32);
		__m256i* dataM = reinterpret_cast<__m256i*>(resultBuffer);
		__m256i four = _mm256_set1_epi32(4);
		__m256i v0 = _mm256_set_epi32(19,3,18,2,17,1,16,0);
		__m256i v1 = _mm256_add_epi32(v0, four);
		__m256i v2 = _mm256_add_epi32(v1, four);
		__m256i v3 = _mm256_add_epi32(v2, four);
		__m256i thirtyTwo = _mm256_slli_epi32(four, 3);
		for(uint32_t blockI = 0; blockI < numBlocks; blockI++) {
			_mm256_stream_si256(dataM++, v0);
			_mm256_stream_si256(dataM++, v1);
			_mm256_stream_si256(dataM++, v2);
			_mm256_stream_si256(dataM++, v3);
			v0 = _mm256_add_epi32(v0, thirtyTwo);
			v1 = _mm256_add_epi32(v1, thirtyTwo);
			v2 = _mm256_add_epi32(v2, thirtyTwo);
			v3 = _mm256_add_epi32(v3, thirtyTwo);
		}
		resultBuffer += numBlocks * FPGA_BLOCK_SIZE;
	}
	for(uint32_t i = numBlocks * FPGA_BLOCK_SIZE; i < fillUpTo; i++) {
		*resultBuffer++ = i;
	}
	/*uint32_t finalNode = mbfCounts[Variables]-1;
	while(reinterpret_cast<std::uintptr_t>(resultBuffer+2) % FPGA_BLOCK_ALIGN != 0) {
		*resultBuffer++ = finalNode;
	}
	*resultBuffer++ = uint32_t(0x80000000);*/
}

static void finalizeMaskBuffers(
	swapper_block bufferMask,
	uint32_t nodeOffset, 
	const JobTopInfo* tops,
	uint32_t* __restrict * __restrict resultBuffers
) {
	while(bufferMask != 0) {
		int idx = ctz64(bufferMask);
		bufferMask &= ~(swapper_block(1) << idx);

		uint32_t finalizeUpTo = nodeOffset;
#ifdef PCOEFF_DEDUPLICATE
		if(tops[idx].topDual < finalizeUpTo) finalizeUpTo = tops[idx].topDual;
#endif
		finalizeBuffer(resultBuffers[idx], finalizeUpTo);
	}
}

static void generateBotBuffers(
	unsigned int Variables, 
	swapper_block* __restrict swapperA,
	swapper_block* __restrict swapperB,
	uint32_t* __restrict * __restrict resultBuffers,
	SynchronizedQueue<JobInfo>& outputQueue,
	const uint32_t* __restrict links,
	const JobTopInfo* tops,
	int numberOfTops
) {
	JobInfo jobs[BUFFERS_PER_BATCH];
	for(int i = 0; i < numberOfTops; i++) {
		jobs[i].bufStart = resultBuffers[i];
		jobs[i].topDual = tops[i].topDual;
	}

	int topLayers[BUFFERS_PER_BATCH];
	int startingLayer = initializeSwapperRun(Variables, swapperA, resultBuffers, tops, topLayers, numberOfTops);

	for(int i = 0; i < numberOfTops; i++) {
		jobs[i].topLayer = topLayers[i];
	}
	
	swapper_block activeMask = numberOfTops == BUFFERS_PER_BATCH ? swapper_block(0xFFFFFFFFFFFFFFFF) : (swapper_block(1) << numberOfTops) - 1; // Has a 1 for active buffers

	// Reverse order of mbf structure for better memory access pattern
	const uint32_t* thisLayerLinks = links + flatLinkOffsets[Variables][(1 << Variables) - startingLayer];
	for(int toLayer = startingLayer - 1; toLayer >= 0; toLayer--) {
		initializeSwapperTops(Variables, swapperA, toLayer+1, tops, topLayers, numberOfTops);

		size_t numberOfLinksToLayer = linkCounts[Variables][toLayer];
		assert(toLayer == (1 << Variables) - 1 || (thisLayerLinks[-1] & uint32_t(0x80000000)) != 0);
		assert((thisLayerLinks[numberOfLinksToLayer-1] & uint32_t(0x80000000)) != 0);

		uint32_t nodeOffset = flatNodeLayerOffsets[Variables][toLayer];
		// Computes BUFFERS_PER_BATCH result buffers at a time
		swapper_block finishedBuffers = computeNextLayer(thisLayerLinks, tops, swapperA, swapperB, nodeOffset, numberOfLinksToLayer, resultBuffers, activeMask
#ifndef NDEBUG
			,layerSizes[Variables][toLayer+1],layerSizes[Variables][toLayer]
#endif
		);

		thisLayerLinks += numberOfLinksToLayer;

		activeMask &= ~finishedBuffers;

		finalizeMaskBuffers(finishedBuffers, nodeOffset, tops, resultBuffers);

		if(activeMask == 0) break;

		std::swap(swapperA, swapperB);
	}

	finalizeMaskBuffers(activeMask, 0, tops, resultBuffers);

	for(int i = 0; i < numberOfTops; i++) {
		jobs[i].bufEnd = resultBuffers[i];
	}
	outputQueue.pushN(jobs, numberOfTops);
}

const uint32_t* loadLinks(unsigned int Variables) {
	return readFlatBuffer<uint32_t>(FileName::mbfStructure(Variables), getTotalLinkCount(Variables));
}

#include <iostream>

static void runBottomBufferCreatorNoAlloc (
	unsigned int Variables,
	std::atomic<const JobTopInfo*>& curStartingJobTop,
	const JobTopInfo* jobTopsEnd,
	SynchronizedQueue<JobInfo>& outputQueue,
	SynchronizedStack<uint32_t*>& returnQueue,
	const uint32_t* links
) {
	size_t SWAPPER_WIDTH = getMaxLayerSize(Variables);
	swapper_block* swapperA = aligned_mallocT<swapper_block>(SWAPPER_WIDTH, 64);
	swapper_block* swapperB = aligned_mallocT<swapper_block>(SWAPPER_WIDTH, 64);

	std::cout << "\033[33m[BottomBufferCreator] Thread Started!\033[39m\n" << std::flush;

	while(true) {
		const JobTopInfo* grabbedTopSet = curStartingJobTop.fetch_add(BUFFERS_PER_BATCH);
		if(grabbedTopSet >= jobTopsEnd) break;

		int numberOfTops = std::min(int(jobTopsEnd - grabbedTopSet), BUFFERS_PER_BATCH);

		//std::cout << std::this_thread::get_id() << " grabbed " << numberOfTops << " tops!" << std::endl;

		uint32_t* buffersEnd[BUFFERS_PER_BATCH];
		returnQueue.popN_wait(buffersEnd, numberOfTops);

		generateBotBuffers(Variables, swapperA, swapperB, buffersEnd, outputQueue, links, grabbedTopSet, numberOfTops);
	}

	std::cout << "\033[33m[BottomBufferCreator] Thread Finished!\033[39m\n" << std::flush;

	aligned_free(swapperA);
	aligned_free(swapperB);
}

void runBottomBufferCreator(
	unsigned int Variables,
	std::future<std::vector<JobTopInfo>>& jobTops,
	SynchronizedQueue<JobInfo>& outputQueue,
	SynchronizedStack<uint32_t*>& returnQueue,
	int numberOfThreads
) {
	setCoreComplexAffinity(0);

	std::cout << "\033[33m[BottomBufferCreator] Loading Links...\033[39m\n" << std::flush;
	auto linkLoadStart = std::chrono::high_resolution_clock::now();
	const uint32_t* links = loadLinks(Variables);
	double timeTaken = (std::chrono::high_resolution_clock::now() - linkLoadStart).count() * 1.0e-9;
	std::cout << "\033[33m[BottomBufferCreator] Finished loading links. Took " + std::to_string(timeTaken) + "s\033[39m\n" << std::flush;

	std::vector<JobTopInfo> jobTopsVec = jobTops.get();
	std::atomic<const JobTopInfo*> jobTopAtomic;
	jobTopAtomic.store(&jobTopsVec[0]);
	const JobTopInfo* jobTopEnd = jobTopAtomic.load() + jobTopsVec.size();

	std::vector<std::thread> threads(numberOfThreads - 1);
	for(int t = 0; t < numberOfThreads - 1; t++) {
		threads[t] = std::thread([&](){runBottomBufferCreatorNoAlloc(Variables, jobTopAtomic, jobTopEnd, outputQueue, returnQueue, links);});
		setCoreComplexAffinity(threads[t], t+1);
	}
	runBottomBufferCreatorNoAlloc(Variables, jobTopAtomic, jobTopEnd, outputQueue, returnQueue, links);

	for(std::thread& t : threads) {
		t.join();
	}
	std::cout << "\033[33m[BottomBufferCreator] All Threads finished! Closing output queue\033[39m\n" << std::flush;

	outputQueue.close();
}
