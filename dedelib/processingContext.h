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
constexpr size_t NUM_INPUT_BUFFERS_PER_NODE = 160; // 320 buffers in total
constexpr size_t NUM_RESULT_BUFFERS_PER_NODE = 60; // 120 buffers in total

class PCoeffProcessingContextEighth {
public:
	// Return queues are implemented as stacks, to try and reuse recently retired buffers more often, to improve cache coherency. 
	SynchronizedQueue<NodeIndex*> inputBufferAlloc;
	SynchronizedQueue<ProcessedPCoeffSum*> resultBufferAlloc;

	SynchronizedQueue<OutputBuffer> outputQueue;
	SynchronizedQueue<OutputBuffer> validationQueue;

	PCoeffProcessingContextEighth();
	~PCoeffProcessingContextEighth();
};

class PCoeffProcessingContext {
public:
	NUMAArray<NodeIndex> numaInputMemory[NUMA_SLICE_COUNT];
	NUMAArray<ProcessedPCoeffSum> numaResultMemory[NUMA_SLICE_COUNT];
	unique_numa_ptr<PCoeffProcessingContextEighth> numaQueues[NUMA_SLICE_COUNT];

	SynchronizedMultiQueue<JobInfo> inputQueue;

	MutexLatch topsAreReady;
	std::vector<JobTopInfo> tops; // Synchronizes on the topsAreReady latch

	void initTops(std::vector<JobTopInfo> tops);

	PCoeffProcessingContext(unsigned int Variables);
	~PCoeffProcessingContext();

	PCoeffProcessingContextEighth& getNUMAForBuf(const NodeIndex* id) const;
	PCoeffProcessingContextEighth& getNUMAForBuf(const ProcessedPCoeffSum* id) const;

	void free(NodeIndex* inputBuf) const;
	void free(ProcessedPCoeffSum* resultBuf) const;
};
