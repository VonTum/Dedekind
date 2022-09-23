#pragma once

#include "synchronizedQueue.h"
#include "pcoeffClasses.h"

class PCoeffProcessingContext {
public:
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
	SynchronizedMultiQueue<JobInfo> inputQueue;

	SynchronizedQueue<OutputBuffer> outputQueue;

	SynchronizedQueue<OutputBuffer> validationQueue;

	// Return queues are implemented as stacks, to try and reuse recently retired buffers more often, to improve cache coherency. 
	SynchronizedMultiNUMAAlloc<NodeIndex> inputBufferAllocator;
	SynchronizedMultiNUMAAlloc<ProcessedPCoeffSum> outputBufferReturnQueue;

	PCoeffProcessingContext(unsigned int Variables, size_t numInputBuffers, size_t numOutputBuffers);
	~PCoeffProcessingContext();
};
