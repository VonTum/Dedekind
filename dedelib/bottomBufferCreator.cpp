#include "bottomBufferCreator.h"

#include "crossPlatformIntrinsics.h"

#include "knownData.h"

#include <xmmintrin.h>
#include <emmintrin.h>
#include <immintrin.h>

#include <sys/mman.h>
#include <string.h>

#include <vector>
#include <algorithm>
#include <atomic>
#include <thread>
#include <chrono>
#include <iostream>


#include "flatBufferManagement.h"
#include "fileNames.h"

#include "aligned_alloc.h"

#include "threadUtils.h"
#include <pthread.h>
#include "threadPool.h"

#include "numaMem.h"

typedef uint8_t swapper_block;
constexpr int BUFFERS_PER_BATCH = sizeof(swapper_block) * 8;
constexpr size_t PREFETCH_OFFSET = 48;

constexpr size_t FPGA_BLOCK_SIZE = 32;
constexpr size_t FPGA_BLOCK_ALIGN = FPGA_BLOCK_SIZE * sizeof(uint32_t);
constexpr size_t JOB_SIZE_ALIGNMENT = 16 * FPGA_BLOCK_SIZE; // Block Size 32, shuffle size 16

/*
 * Swapper code
 */

static uint32_t* convertBlockPtr(uint32_t* ptr) {
	static_assert(sizeof(uint32_t) == 4);
	uintptr_t p = reinterpret_cast<uintptr_t>(ptr);
	constexpr uintptr_t LOWER_MASK = (FPGA_BLOCK_SIZE / 2 - 1) * sizeof(uint32_t);
	static_assert(LOWER_MASK == 0b00111100);
	constexpr uintptr_t MSB_MASK = (FPGA_BLOCK_SIZE / 2) * sizeof(uint32_t);
	static_assert(MSB_MASK == 0b01000000);

	uintptr_t cvtP = (p & ~(LOWER_MASK | MSB_MASK)) | ((p & LOWER_MASK) << 1) | ((p & MSB_MASK) >> 4);

	return reinterpret_cast<uint32_t*>(cvtP);
}

static void unshuffleBlockForFPGA(uint32_t* block) {
	assert(reinterpret_cast<uintptr_t>(block) % FPGA_BLOCK_ALIGN == 0);
	alignas(FPGA_BLOCK_ALIGN) uint32_t tmpBuf[FPGA_BLOCK_SIZE];
	for(size_t i = 0; i < FPGA_BLOCK_SIZE; i++) {
		tmpBuf[i] = block[i];
	}
	for(size_t i = 0; i < FPGA_BLOCK_SIZE; i++) {
		block[i] = *convertBlockPtr(&tmpBuf[i]);
	}
}

static void addValueToBlockFPGA(uint32_t*& buf, uint32_t newValue) {
	//*buf = newValue;
	_mm_stream_si32(reinterpret_cast<int*>(convertBlockPtr(buf)), static_cast<int>(newValue));
	//_mm_stream_si32(reinterpret_cast<int*>(buf), static_cast<int>(newValue)); 
	buf++;
}

// Returns the starting layer aka the highest layer
static int initializeSwapperRun(
	unsigned int Variables,
	JobInfo* __restrict resultBuffers,
	const JobTopInfo* __restrict tops,
	int* topLayers,
	int numberOfTops
) {
	int highestLayer = 0;
	for(int i = 0; i < numberOfTops; i++) {
		uint32_t curTop = tops[i].top;
		uint32_t topDual = tops[i].topDual;
		int layer = getFlatLayerOfIndex(Variables, curTop);
		if(layer > highestLayer) highestLayer = layer;
		topLayers[i] = layer;

		addValueToBlockFPGA(resultBuffers[i].bufEnd, curTop | uint32_t(0x80000000)); // Mark the incoming top so that the processor can detect it
		addValueToBlockFPGA(resultBuffers[i].bufEnd, curTop | uint32_t(0x80000000)); // Double because the FPGA is double-pumped

#ifdef PCOEFF_DEDUPLICATE
		addValueToBlockFPGA(resultBuffers[i].bufEnd, topDual); // Add dual in fixed location. That way it doesn't need to be computed by resultsCollection
		if(curTop < topDual)
#endif
			addValueToBlockFPGA(resultBuffers[i].bufEnd, curTop);
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



static void finalizeBuffer(unsigned int Variables, JobInfo& resultBuffer, const JobTopInfo& topInfo, uint32_t nodeOffset) {
	uint32_t fillUpTo = nodeOffset;
#ifdef PCOEFF_DEDUPLICATE
	if(topInfo.topDual < fillUpTo) fillUpTo = topInfo.topDual;
#endif

	if(fillUpTo >= FPGA_BLOCK_SIZE) {
		uintptr_t alignOffset = (reinterpret_cast<uintptr_t>(resultBuffer.bufEnd) / sizeof(uint32_t)) % FPGA_BLOCK_SIZE;
		if(alignOffset != 0) {
			uint32_t alignmentElementCount = FPGA_BLOCK_SIZE - alignOffset;
			fillUpTo = fillUpTo - alignmentElementCount;
			for(uint32_t i = 0; i < alignmentElementCount; i++) {
				addValueToBlockFPGA(resultBuffer.bufEnd, fillUpTo + i);
			}
		}
	} else {
		uint32_t* alignedEndPtr = reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(resultBuffer.bufEnd) & ~(FPGA_BLOCK_ALIGN - 1));
		if(alignedEndPtr != resultBuffer.bufStart) {
			unshuffleBlockForFPGA(alignedEndPtr); // Have to unshuffle last segment too, if it wasn't aligned by the above code
		}
	}
	unshuffleBlockForFPGA(resultBuffer.bufStart);
	// Generate aligned blocks efficiently
	uint32_t numBlocks = fillUpTo / FPGA_BLOCK_SIZE;
	if(numBlocks >= 1) {
		static_assert(FPGA_BLOCK_SIZE == 32);
		__m256i* dataM = reinterpret_cast<__m256i*>(resultBuffer.bufEnd);
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
		resultBuffer.bufEnd += numBlocks * FPGA_BLOCK_SIZE;
	}

	// Last chunk should be ordered properly
	for(uint32_t i = numBlocks * FPGA_BLOCK_SIZE; i < fillUpTo; i++) {
		//*resultBuffer.bufEnd = i;
		_mm_stream_si32(reinterpret_cast<int*>(resultBuffer.bufEnd), static_cast<int>(i));
		resultBuffer.bufEnd++;
	}
	/*uint32_t finalNode = mbfCounts[Variables]-1;
	while(reinterpret_cast<std::uintptr_t>(resultBuffer.bufEnd+2) % FPGA_BLOCK_ALIGN != 0) {
		*resultBuffer.bufEnd++ = finalNode;
	}
	*resultBuffer.bufEnd++ = uint32_t(0x80000000);*/

	// Add end cap
	resultBuffer.blockEnd = resultBuffer.bufEnd;
	uintptr_t endAlign = (reinterpret_cast<uintptr_t>(resultBuffer.blockEnd) / sizeof(uint32_t)) % JOB_SIZE_ALIGNMENT;
	if(endAlign != 0) {
		for(; endAlign < JOB_SIZE_ALIGNMENT; endAlign++) {
			_mm_stream_si32(reinterpret_cast<int*>(resultBuffer.blockEnd++), static_cast<int>(mbfCounts[Variables] - 1)); // Fill with the global TOP mbf. AKA 0xFFFFFFFF.... to minimize wasted work
		}
	}
	assert((reinterpret_cast<uintptr_t>(resultBuffer.blockEnd) / sizeof(uint32_t)) % FPGA_BLOCK_ALIGN == 0);
	/*for(size_t i = 0; i < 2; i++) {
		resultBuffer.bufStart[bufferSize++] = 0x80000000; // Tops at the end for stats collection
	}*/

	/*int bufSize = resultBuffer.getNumberOfBottoms() + 2;
	std::string text = "\033[34mBuf: (size=" + std::to_string(bufSize) + ") ";
	for(int i = 0; i < 64; i++) {
		if(i == bufSize) text.append("\033[39m");
		text.append(std::to_string(resultBuffer.bufStart[i]));
		text.append(", ");
	}
	text.append("\033[39m\n");
	std::cout << text << std::flush;*/
}

static void finalizeBuffersMasked(
	unsigned int Variables,
	swapper_block bufferMask,
	uint32_t nodeOffset, 
	const JobTopInfo* tops,
	JobInfo* __restrict resultBuffers
) {
	while(bufferMask != 0) {
		static_assert(BUFFERS_PER_BATCH == 8);
		int idx = ctz8(bufferMask);
		bufferMask &= ~(swapper_block(1) << idx);

		finalizeBuffer(Variables, resultBuffers[idx], tops[idx], nodeOffset);
	}
}

static void computeNextLayerLinks (
	const uint32_t* __restrict links, // Links index from the previous layer to this layer. Last element of a link streak will have a 1 in the 31 bit position. 
	const swapper_block* __restrict swapperIn,
	swapper_block* __restrict swapperOut,
	uint32_t numberOfLinksToLayer
#ifndef NDEBUG
	,uint32_t fromLayerSize,
	uint32_t toLayerSize
#endif
) {
	swapper_block currentConnectionsIn = 0;
	uint32_t curNodeI = 0;
	for(uint32_t linkI = 0; linkI < numberOfLinksToLayer; linkI++) {
		uint32_t curLink = links[linkI];
		uint32_t fromIdx = curLink & uint32_t(0x7FFFFFFF);
		assert(fromIdx < fromLayerSize);
		currentConnectionsIn |= swapperIn[fromIdx];
		if((curLink & uint32_t(0x80000000)) != 0) {
			assert(curNodeI < toLayerSize);
			swapperOut[curNodeI] = currentConnectionsIn;
			currentConnectionsIn = 0;
			curNodeI++;
		}

		// Prefetch a bit in advance
		if constexpr(PREFETCH_OFFSET > 0) {
			uint32_t prefetchLinkI = linkI + PREFETCH_OFFSET;
			uint32_t prefetchLink = links[prefetchLinkI];
			uint32_t prefetchIdx = prefetchLink & uint32_t(0x7FFFFFFF);
			_mm_prefetch(swapperIn + prefetchIdx, _MM_HINT_T0);
		}
	}

	assert(curNodeI == toLayerSize);
}

static swapper_block pushSwapperResults(
	unsigned int Variables,
	const JobTopInfo* tops,
	const swapper_block* __restrict swapper,
	uint32_t resultIndexOffset,
	JobInfo* __restrict resultBuffers,
	swapper_block activeMask, // Has a 1 for active buffers
	uint32_t swapperSize
) {
	swapper_block finishedConnections = activeMask;

	for(uint32_t curNodeI = 0; curNodeI < swapperSize; curNodeI++) {
		swapper_block currentConnectionsIn = swapper[curNodeI];
		finishedConnections &= currentConnectionsIn;
	}

	for(size_t bufI = 0; bufI < BUFFERS_PER_BATCH; bufI++) {
		swapper_block bufBit = swapper_block(1) << bufI;
		if((activeMask & bufBit) == 0) continue;
		uint32_t curTopDual = tops[bufI].topDual;
		uint32_t*& curBufEnd = resultBuffers[bufI].bufEnd;

		uint32_t upTo = resultIndexOffset + swapperSize;
#ifdef PCOEFF_DEDUPLICATE
		if(curTopDual < upTo) {
			upTo = curTopDual;
		}
#endif

		for(uint32_t thisValue = resultIndexOffset; thisValue < upTo; thisValue++) {
			if((swapper[thisValue - resultIndexOffset] & bufBit) != 0) {
				addValueToBlockFPGA(curBufEnd, thisValue);
			}
		}
	}

	finalizeBuffersMasked(Variables, finishedConnections, resultIndexOffset, tops, resultBuffers);

	return activeMask & ~finishedConnections;
}

static void generateBotBuffers(
	unsigned int Variables, 
	swapper_block* __restrict swapperA,
	swapper_block* __restrict swapperB,
	uint32_t* __restrict * __restrict resultBuffers,
	SynchronizedMultiQueue<JobInfo>& outputQueue,
	size_t socket,
	const uint32_t* __restrict links,
	const JobTopInfo* tops,
	int numberOfTops
) {
	memset(swapperA, 0, sizeof(swapper_block) * getMaxLayerSize(Variables));

	JobInfo jobs[BUFFERS_PER_BATCH];
	for(int i = 0; i < numberOfTops; i++) {
		jobs[i].bufStart = resultBuffers[i];
		jobs[i].bufEnd = resultBuffers[i];
	}

	int topLayers[BUFFERS_PER_BATCH];
	int startingLayer = initializeSwapperRun(Variables, jobs, tops, topLayers, numberOfTops);

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
		computeNextLayerLinks(thisLayerLinks, swapperA, swapperB, numberOfLinksToLayer
#ifndef NDEBUG
			,layerSizes[Variables][toLayer+1],layerSizes[Variables][toLayer]
#endif
		);

		activeMask = pushSwapperResults(Variables, tops, swapperB, nodeOffset, jobs, activeMask, layerSizes[Variables][toLayer]);

		if(activeMask == 0) break;

		thisLayerLinks += numberOfLinksToLayer;

		std::swap(swapperA, swapperB);
	}

	finalizeBuffersMasked(Variables, activeMask, 0, tops, jobs);

	outputQueue.pushN(socket, jobs, numberOfTops);
}

static void runBottomBufferCreatorNoAlloc (
	unsigned int Variables,
	std::atomic<const JobTopInfo*>& curStartingJobTop,
	const JobTopInfo* jobTopsEnd,
	PCoeffProcessingContext& context,
	size_t coreComplex,
	std::atomic<const uint32_t*>& links
) {
	size_t SWAPPER_WIDTH = getMaxLayerSize(Variables);
	swapper_block* swapperA = aligned_mallocT<swapper_block>(SWAPPER_WIDTH, 64);
	swapper_block* swapperB = aligned_mallocT<swapper_block>(SWAPPER_WIDTH, 64);

	std::cout << "\033[33m[BottomBufferCreator " + std::to_string(coreComplex) + "] Thread Started!\033[39m\n" << std::flush;

	size_t socket = coreComplex / 8;
	PCoeffProcessingContextEighth& subContext = *context.numaQueues[socket];

	while(true) {
		const JobTopInfo* grabbedTopSet = curStartingJobTop.fetch_add(BUFFERS_PER_BATCH);
		if(grabbedTopSet >= jobTopsEnd) break;

		int numberOfTops = std::min(int(jobTopsEnd - grabbedTopSet), BUFFERS_PER_BATCH);

		//std::cout << std::this_thread::get_id() << " grabbed " << numberOfTops << " tops!" << std::endl;

		uint32_t* buffersEnd[BUFFERS_PER_BATCH];
		subContext.inputBufferAlloc.popN_wait(buffersEnd, numberOfTops);

		generateBotBuffers(Variables, swapperA, swapperB, buffersEnd, context.inputQueue, socket, links.load(), grabbedTopSet, numberOfTops);

		std::cout << "\033[33m[BottomBufferCreator " + std::to_string(coreComplex) + "] Pushed 8 Buffers\033[39m\n" << std::flush;
	}

	std::cout << "\033[33m[BottomBufferCreator " + std::to_string(coreComplex) + "] Thread Finished!\033[39m\n" << std::flush;

	aligned_free(swapperA);
	aligned_free(swapperB);
}

const uint32_t* loadLinks(unsigned int Variables) {
	return readFlatBuffer<uint32_t>(FileName::mbfStructure(Variables), getTotalLinkCount(Variables));
}

constexpr size_t BOTTOM_BUF_CREATOR_COUNT = 16;
void runBottomBufferCreator(
	unsigned int Variables,
	PCoeffProcessingContext& context
) {
	std::cout << "\033[33m[BottomBufferCreator] Loading Links...\033[39m\n" << std::flush;
	auto linkLoadStart = std::chrono::high_resolution_clock::now();
	//const uint32_t* links = loadLinks(Variables);
	
	size_t linkBufMemSize = getTotalLinkCount(Variables) * sizeof(uint32_t);
	size_t linkBufMemSizeWithPrefetching = linkBufMemSize + PREFETCH_OFFSET * sizeof(uint32_t);
	
	void* numaLinks[2];
	allocSocketBuffers(linkBufMemSizeWithPrefetching, numaLinks);
	readFlatVoidBufferNoMMAP(FileName::mbfStructure(Variables), linkBufMemSize, numaLinks[1]);
	memset((char*) numaLinks[1] + linkBufMemSize, 0, PREFETCH_OFFSET * sizeof(uint32_t));

	std::atomic<const uint32_t*> links[2];
	links[0].store(reinterpret_cast<const uint32_t*>(numaLinks[1])); // Not a mistake, gets replaced by numaLinks[1] after it is copied
	links[1].store(reinterpret_cast<const uint32_t*>(numaLinks[1]));
	
	double timeTaken = (std::chrono::high_resolution_clock::now() - linkLoadStart).count() * 1.0e-9;
	std::cout << "\033[33m[BottomBufferCreator] Finished loading links. Took " + std::to_string(timeTaken) + "s\033[39m\n" << std::flush;

	std::atomic<const JobTopInfo*> jobTopAtomic;
	context.topsAreReady.wait();
	jobTopAtomic.store(&context.tops[0]);
	const JobTopInfo* jobTopEnd = jobTopAtomic.load() + context.tops.size();

	struct ThreadInfo {
		unsigned int Variables;
		std::atomic<const JobTopInfo*>* curStartingJobTop;
		const JobTopInfo* jobTopsEnd;
		PCoeffProcessingContext* context;
		size_t coreComplex;
		std::atomic<const uint32_t*>* links;
	};

	auto threadFunc = [](void* data) -> void* {
		ThreadInfo* ti = (ThreadInfo*) data;
		std::string threadName = "BotBufCrea " + std::to_string(ti->coreComplex);
		setThreadName(threadName.c_str());
		runBottomBufferCreatorNoAlloc(ti->Variables, *ti->curStartingJobTop, ti->jobTopsEnd, *ti->context, ti->coreComplex, *ti->links);
		pthread_exit(NULL);
		return NULL;
	};

	ThreadInfo threadDatas[BOTTOM_BUF_CREATOR_COUNT];
	for(size_t coreComplex = 0; coreComplex < BOTTOM_BUF_CREATOR_COUNT; coreComplex++) {
		size_t socket = coreComplex / 8;
		ThreadInfo& ti = threadDatas[coreComplex];
		ti.Variables = Variables;
		ti.curStartingJobTop = &jobTopAtomic;
		ti.jobTopsEnd = jobTopEnd;
		ti.context = &context;
		ti.coreComplex = coreComplex;
		ti.links = &links[socket];
	}

	PThreadBundle threads = spreadThreads(BOTTOM_BUF_CREATOR_COUNT, CPUAffinityType::COMPLEX, threadDatas, threadFunc, 1);

	memcpy(numaLinks[0], numaLinks[1], linkBufMemSizeWithPrefetching);
	links[0].store(reinterpret_cast<const uint32_t*>(numaLinks[0])); // Switch to closer buffer

	std::cout << "\033[33m[BottomBufferCreator] Copied Links to second socket buffer\033[39m\n" << std::flush;

	threads.join();

	std::cout << "\033[33m[BottomBufferCreator] All Threads finished! Closing output queue\033[39m\n" << std::flush;

	context.inputQueue.close();
	numa_free(numaLinks[0], linkBufMemSizeWithPrefetching);
	numa_free(numaLinks[1], linkBufMemSizeWithPrefetching);
}

std::vector<JobTopInfo> convertTopInfos(const FlatNode* flatNodes, const std::vector<NodeIndex>& topIndices) {
	std::vector<JobTopInfo> topInfos;
	topInfos.reserve(topIndices.size());
	for(NodeIndex topIdx : topIndices) {
		JobTopInfo newInfo;
		newInfo.top = topIdx;
		newInfo.topDual = flatNodes[topIdx].dual;
		topInfos.push_back(newInfo);
	}
	return topInfos;
}
