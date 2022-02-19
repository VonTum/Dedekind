#include "flatPCoeffProcessing.h"

#include "aligned_alloc.h"

// Try for at least 2MB huge pages
// Also alignment is required for openCL buffer sending and receiving methods
constexpr size_t ALLOC_ALIGN = 1 << 21;

PCoeffProcessingContext::PCoeffProcessingContext(unsigned int Variables, size_t numberOfInputBuffers, size_t numberOfOutputBuffers) :
	inputQueue(numberOfInputBuffers),
	outputQueue(std::min(numberOfInputBuffers, numberOfOutputBuffers)),
	inputBufferReturnQueue(),
	outputBufferReturnQueue() {
	std::cout << "Allocating " << numberOfInputBuffers << " input buffers." << std::endl;
	for(size_t i = 0; i < numberOfInputBuffers; i++) {
		inputBufferReturnQueue.push(static_cast<NodeIndex*>(aligned_malloc(sizeof(NodeIndex) * MAX_BUFSIZE(Variables), ALLOC_ALIGN)));
	}

	std::cout << "Allocating " << numberOfOutputBuffers << " output buffers." << std::endl;
	for(size_t i = 0; i < numberOfOutputBuffers; i++) {
		outputBufferReturnQueue.push(static_cast<ProcessedPCoeffSum*>(aligned_malloc(sizeof(ProcessedPCoeffSum) * MAX_BUFSIZE(Variables), ALLOC_ALIGN)));
	}
}
PCoeffProcessingContext::~PCoeffProcessingContext() {
	std::cout << "Deleting input buffers..." << std::endl;
	size_t numFreedInputBuffers = 0;
	auto& inputBufferReturnStackContainer = inputBufferReturnQueue.get();
	while(!inputBufferReturnStackContainer.empty()) {
		aligned_free(inputBufferReturnStackContainer.top());
		inputBufferReturnStackContainer.pop();
		numFreedInputBuffers++;
	}
	std::cout << "Deleted " << numFreedInputBuffers << " input buffers. " << std::endl;

	std::cout << "Deleting output buffers..." << std::endl;
	size_t numFreedOutputBuffers = 0;
	auto& outputBufferReturnStackContainer = outputBufferReturnQueue.get();
	while(!outputBufferReturnStackContainer.empty()) {
		aligned_free(outputBufferReturnStackContainer.top());
		outputBufferReturnStackContainer.pop();
		numFreedOutputBuffers++;
	}
	std::cout << "Deleted " << numFreedOutputBuffers << " output buffers. " << std::endl;
}
