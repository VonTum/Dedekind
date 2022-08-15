#include "processingContext.h"

#include <iostream>

#include "aligned_alloc.h"
#include "knownData.h"

// Try for at least 2MB huge pages
// Also alignment is required for openCL buffer sending and receiving methods
constexpr size_t ALLOC_ALIGN = 1 << 21;

static size_t getAlignedBufferSize(unsigned int Variables) {
	return alignUpTo(getMaxDeduplicatedBufferSize(Variables)+20, size_t(32) * 16);
}

PCoeffProcessingContext::PCoeffProcessingContext(unsigned int Variables, size_t numberOfNUMANodes, size_t numberOfInputBuffersPerNUMANode, size_t numOutputBuffers) :
	inputQueue(numberOfNUMANodes, numberOfInputBuffersPerNUMANode),
	outputQueue(std::min(numberOfNUMANodes * numberOfInputBuffersPerNUMANode, numOutputBuffers)),
	inputBufferAllocator(getAlignedBufferSize(Variables), numberOfInputBuffersPerNUMANode, numberOfNUMANodes),
	outputBufferReturnQueue(numOutputBuffers) {
	
	std::cout 
		<< "Create PCoeffProcessingContext with " 
		<< Variables 
		<< " Variables, " 
		<< numberOfNUMANodes 
		<< " NUMA nodes, " 
		<< numberOfInputBuffersPerNUMANode
		<< " buffers / NUMA node, and "
		<< numOutputBuffers
		<< " output buffers" << std::endl;

	std::cout << "Allocating " << numOutputBuffers << " output buffers." << std::endl;
	for(size_t i = 0; i < numOutputBuffers; i++) {
		outputBufferReturnQueue.push(static_cast<ProcessedPCoeffSum*>(aligned_malloc(sizeof(ProcessedPCoeffSum) * MAX_BUFSIZE(Variables), ALLOC_ALIGN)));
	}
}

PCoeffProcessingContext::~PCoeffProcessingContext() {
	std::cout << "Deleting output buffers..." << std::endl;
	size_t numFreedOutputBuffers = 0;
	for(size_t i = 0; i < outputBufferReturnQueue.sz; i++) {
		aligned_free(outputBufferReturnQueue[i]);
	}
	std::cout << "Deleted " << outputBufferReturnQueue.sz << " output buffers. " << std::endl;
	std::cout << "Deleting input buffers..." << std::endl;
}
