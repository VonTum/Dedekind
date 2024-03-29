#pragma once

#include "synchronizedQueue.h"
#include "pcoeffClasses.h"
#include "latch.h"

/*
	This is a closed-loop buffer circulation system. 
	Buffers are allocated once at the start of the program, 
	and are then passed from module to module. 
	There are two classes of buffers in circulation:
		- NodeIndex* buffers: These contain the MBF indices 
								of the bottoms that correspond
								to the top, which is the first
								element. 
		- ProcessedPCoeffSum* buffers:  These contain the results 
										of processPCoeffSum on
										each of the inputs. 
*/

constexpr int NUMA_SLICE_COUNT = 2;

class PCoeffProcessingContextEighth {
public:
	// Return queues are implemented as stacks, to try and reuse recently retired buffers more often, to improve cache coherency. 
	SynchronizedQueue<NodeIndex*> inputBufferAlloc;
	SynchronizedQueue<ProcessedPCoeffSum*> resultBufferAlloc;

	SynchronizedQueue<OutputBuffer> outputQueue;
	SynchronizedQueue<OutputBuffer> validationQueue;

	PCoeffProcessingContextEighth();
	~PCoeffProcessingContextEighth();

	void freeBuf(NodeIndex* bufToFree, size_t bufSize);
	void freeBuf(ProcessedPCoeffSum* bufToFree, size_t bufSize);
};

class PCoeffProcessingContext {
public:
	unsigned int Variables;
	NodeIndex* numaInputMemory[NUMA_SLICE_COUNT];
	ProcessedPCoeffSum* numaResultMemory[NUMA_SLICE_COUNT];
	unique_numa_ptr<PCoeffProcessingContextEighth> numaQueues[NUMA_SLICE_COUNT];

	SynchronizedMultiQueue<JobInfo> inputQueue;

	MutexLatch topsAreReady;
	std::vector<JobTopInfo> tops; // Synchronizes on the topsAreReady latch

	MutexLatch mbfs0Ready;
	MutexLatch mbfsBothReady;
	const void* mbfs[2]; // One buffer on Socket 0 and one on Socket 1

	void initTops(std::vector<JobTopInfo> tops);
	void initMBFS();

	PCoeffProcessingContext(unsigned int Variables);
	~PCoeffProcessingContext();

	PCoeffProcessingContextEighth& getNUMAForBuf(const NodeIndex* id) const;
	PCoeffProcessingContextEighth& getNUMAForBuf(const ProcessedPCoeffSum* id) const;
};
