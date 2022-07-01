#include "bottomBufferCreator.h"

#include "crossPlatformIntrinsics.h"

#include "knownData.h"

#include <emmintrin.h>

#include <cstring>
#include <vector>
#include <algorithm>

#include "flatBufferManagement.h"
#include "fileNames.h"


/*
 * Swapper code
 */

// Returns the starting layer aka the highest layer
static int initializeSwapperRun(
	unsigned int Variables,
	uint64_t* __restrict swapper,
	uint32_t* __restrict * __restrict resultBuffers,
	const JobTopInfo* __restrict tops,
	int* topLayers,
	int numberOfTops
) {
	std::memset(swapper, 0, sizeof(uint64_t) * getMaxLayerSize(Variables));

	int highestLayer = 0;
	for(int i = 0; i < numberOfTops; i++) {
		uint32_t curTop = tops[i].top;
		int layer = getFlatLayerOfIndex(Variables, curTop);
		if(layer > highestLayer) highestLayer = layer;
		topLayers[i] = layer;

		*resultBuffers[i]++ = curTop | uint32_t(0x80000000); // Mark the incoming top so that the processor can detect it
		*resultBuffers[i]++ = curTop | uint32_t(0x80000000); // Double because the FPGA is double-pumped

#ifdef PCOEFF_DEDUPLICATE
		if(curTop < tops[i].topDual)
#endif
			*resultBuffers[i]++ = curTop;
	}


	return highestLayer;
}

static void initializeSwapperTops(
	unsigned int Variables,
	uint64_t* __restrict swapper,
	int currentLayer,
	const JobTopInfo* __restrict tops,
	const int* topLayers,
	int numberOfTops
) {
	for(int topI = 0; topI < numberOfTops; topI++) {
		if(topLayers[topI] == currentLayer) {
			uint32_t topIdxInLayer = tops[topI].top - flatNodeLayerOffsets[Variables][currentLayer];
			swapper[topIdxInLayer] |= uint64_t(1) << topI;
		}
	}
}

// Computes 64 result buffers at a time
static uint64_t computeNextLayer(
	const uint32_t* __restrict links, // Links index from the previous layer to this layer. Last element of a link streak will have a 1 in the 31 bit position. 
	const JobTopInfo* tops,
	const uint64_t* __restrict swapperIn,
	uint64_t* __restrict swapperOut,
	uint32_t resultIndexOffset,
	uint32_t numberOfLinksToLayer,
	uint32_t* __restrict * __restrict resultBuffers,
	uint64_t activeMask = 0xFFFFFFFFFFFFFFFF // Has a 1 for active buffers
#ifndef NDEBUG
	,uint32_t fromLayerSize = 0,
	uint32_t toLayerSize = 0
#endif
) {
	uint64_t finishedConnections = ~uint64_t(0);

	uint64_t currentConnectionsIn = 0;
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
				int idx = ctz64(currentConnectionsIn);
				currentConnectionsIn &= ~(uint64_t(1) << idx);

#ifdef PCOEFF_DEDUPLICATE
				if(thisValue < tops[idx].topDual)
#endif
					//*resultBuffers[idx]++ = thisValue;
					_mm_stream_si32(reinterpret_cast<int*>(resultBuffers[idx]++), static_cast<int>(thisValue)); // TODO Compare performance
			}
		}
	}

	assert(curNodeI == toLayerSize);

	return finishedConnections;
}

static void finalizeBuffer(unsigned int Variables, uint32_t* __restrict & resultBuffer, uint32_t fillUpTo) {
	for(uint32_t i = 0; i < fillUpTo; i++) {
		*resultBuffer++ = i;
	}
	/*uint32_t finalNode = mbfCounts[Variables]-1;
	while(reinterpret_cast<std::uintptr_t>(resultBuffer+2) % 64 != 0) {
		*resultBuffer++ = finalNode;
	}
	*resultBuffer++ = uint32_t(0x80000000);*/
}

static void generateBotBuffers(
	unsigned int Variables, 
	uint64_t* __restrict swapperA,
	uint64_t* __restrict swapperB,
	uint32_t* __restrict * __restrict resultBuffers,
	SynchronizedQueue<JobInfo>& outputQueue,
	const uint32_t* __restrict links,
	const JobTopInfo* tops,
	int numberOfTops
) {
	JobInfo jobs[64];
	for(size_t i = 0; i < numberOfTops; i++) {
		jobs[i].bufStart = resultBuffers[i];
		jobs[i].topDual = tops[i].topDual;
	}

	int topLayers[64];
	int startingLayer = initializeSwapperRun(Variables, swapperA, resultBuffers, tops, topLayers, numberOfTops);

	for(size_t i = 0; i < numberOfTops; i++) {
		jobs[i].topLayer = topLayers[i];
	}
	
	uint64_t activeMask = numberOfTops == 64 ? 0xFFFFFFFFFFFFFFFF : (uint64_t(1) << numberOfTops) - 1; // Has a 1 for active buffers

	for(int toLayer = startingLayer - 1; toLayer >= 0; toLayer--) {
		initializeSwapperTops(Variables, swapperA, toLayer+1, tops, topLayers, numberOfTops);

		const uint32_t* thisLayerLinks = links + flatLinkOffsets[Variables][toLayer];
		size_t numberOfLinksToLayer = linkCounts[Variables][toLayer];
		assert(toLayer == 0 || (thisLayerLinks[-1] & uint32_t(0x80000000)) != 0);
		assert((thisLayerLinks[numberOfLinksToLayer-1] & uint32_t(0x80000000)) != 0);

		uint32_t nodeOffset = flatNodeLayerOffsets[Variables][toLayer];
		// Computes 64 result buffers at a time
		uint64_t finishedBuffers = computeNextLayer(thisLayerLinks, tops, swapperA, swapperB, nodeOffset, numberOfLinksToLayer, resultBuffers, activeMask
#ifndef NDEBUG
			,layerSizes[Variables][toLayer+1],layerSizes[Variables][toLayer]
#endif
		);

		activeMask &= ~finishedBuffers;

		while(finishedBuffers != 0) {
			int idx = ctz64(finishedBuffers);
			finishedBuffers &= ~(uint64_t(1) << idx);

			size_t finalizeUpTo = nodeOffset;
#ifdef PCOEFF_DEDUPLICATE
			if(tops[idx].topDual < finalizeUpTo) finalizeUpTo = tops[idx].topDual;
#endif
			finalizeBuffer(Variables, resultBuffers[idx], finalizeUpTo);

			jobs[idx].bufEnd = resultBuffers[idx];
			outputQueue.push(jobs[idx]);
		}

		if(activeMask == 0) break;

		std::swap(swapperA, swapperB);
	}
}

const uint32_t* loadLinks(unsigned int Variables) {
	return readFlatBuffer<uint32_t>(FileName::mbfStructure(Variables), getTotalLinkCount(Variables));
}

void runBottomBufferCreatorNoAlloc (
	unsigned int Variables,
	const std::vector<JobTopInfo>& jobTops,
	SynchronizedQueue<JobInfo>& outputQueue,
	SynchronizedStack<uint32_t*>& returnQueue,
	const uint32_t* links,
	uint64_t* swapperA,
	uint64_t* swapperB
) {
	uint32_t* buffersEnd[64];
	for(size_t startingTop = 0; startingTop < jobTops.size(); startingTop += 64) {
		const JobTopInfo* curJobSet = &jobTops[startingTop];

		size_t numberOfTops = std::min(jobTops.size() - startingTop, size_t(64));

		returnQueue.popN_wait(buffersEnd, numberOfTops);

		generateBotBuffers(Variables, swapperA, swapperB, buffersEnd, outputQueue, links, curJobSet, numberOfTops);
	}

	outputQueue.close();
}

void runBottomBufferCreator(
	unsigned int Variables,
	const std::vector<JobTopInfo>& jobTops,
	SynchronizedQueue<JobInfo>& outputQueue,
	SynchronizedStack<uint32_t*>& returnQueue 
) {
	const uint32_t* links = loadLinks(Variables);

	size_t SWAPPER_WIDTH = getMaxLayerSize(Variables);
	uint64_t* swapperA = new uint64_t[SWAPPER_WIDTH];
	uint64_t* swapperB = new uint64_t[SWAPPER_WIDTH];

	runBottomBufferCreatorNoAlloc(Variables, jobTops, outputQueue, returnQueue, links, swapperA, swapperB);

	delete[] swapperA;
	delete[] swapperB;
}
