#pragma once

#include "synchronizedQueue.h"
#include "pcoeffClasses.h"

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

class PCoeffProcessingContextEighth {
public:
	// Return queues are implemented as stacks, to try and reuse recently retired buffers more often, to improve cache coherency. 
	SynchronizedStack<NodeIndex*> inputBufferAlloc;
	SynchronizedStack<ProcessedPCoeffSum*> resultBufferAlloc;

	SynchronizedQueue<OutputBuffer> outputQueue;
	SynchronizedQueue<OutputBuffer> validationQueue;

	PCoeffProcessingContextEighth(size_t numInputBuffers, size_t numResultBuffers);
};

class PCoeffProcessingContext {
public:
	NUMAArray<NodeIndex> numaInputMemory[8];
	NUMAArray<ProcessedPCoeffSum> numaResultMemory[8];
	unique_numa_ptr<PCoeffProcessingContextEighth> numaQueues[8];

	SynchronizedMultiQueue<JobInfo> inputQueue;

	PCoeffProcessingContext(unsigned int Variables, size_t numInputBuffers, size_t numResultBuffers);
	~PCoeffProcessingContext();

	PCoeffProcessingContextEighth& getNUMAForBuf(const NodeIndex* id) const;
	PCoeffProcessingContextEighth& getNUMAForBuf(const ProcessedPCoeffSum* id) const;

	void free(NodeIndex* inputBuf) const;
	void free(ProcessedPCoeffSum* resultBuf) const;
};
