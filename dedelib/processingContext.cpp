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

PCoeffProcessingContext::PCoeffProcessingContext(unsigned int Variables, size_t numInputBuffers, size_t numOutputBuffers) :
	inputQueue(8, numInputBuffers),
	outputQueue(std::min(numInputBuffers, numOutputBuffers)),
	inputBufferAllocator(getAlignedBufferSize(Variables), numInputBuffers / 8, 8),
	outputBufferReturnQueue(getAlignedBufferSize(Variables), numOutputBuffers / 2, 2, true) {
	
	std::cout 
		<< "Create PCoeffProcessingContext with " 
		<< Variables 
		<< " Variables, " 
		<< numInputBuffers
		<< " buffers / NUMA node, and "
		<< numOutputBuffers
		<< " output buffers" << std::endl;
}

PCoeffProcessingContext::~PCoeffProcessingContext() {
	std::cout << "Destroy PCoeffProcessingContext, Deleting input and output buffers..." << std::endl;
}
