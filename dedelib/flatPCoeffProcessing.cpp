#include "flatPCoeffProcessing.h"

PCoeffProcessingContext::PCoeffProcessingContext(unsigned int Variables, size_t numberOfInputBuffers, size_t numberOfOutputBuffers) :
	inputQueue(numberOfInputBuffers),
	outputQueue(std::min(numberOfInputBuffers, numberOfOutputBuffers)),
	inputBufferReturnQueue(numberOfInputBuffers),
	outputBufferReturnQueue(numberOfOutputBuffers) {
	std::cout << "Allocating " << numberOfInputBuffers << " input buffers." << std::endl;
	for(size_t i = 0; i < numberOfInputBuffers; i++) {
		inputBufferReturnQueue.push(static_cast<NodeIndex*>(malloc(sizeof(NodeIndex) * MAX_BUFSIZE(Variables))));
	}

	std::cout << "Allocating " << numberOfOutputBuffers << " output buffers." << std::endl;
	for(size_t i = 0; i < numberOfOutputBuffers; i++) {
		outputBufferReturnQueue.push(static_cast<ProcessedPCoeffSum*>(malloc(sizeof(ProcessedPCoeffSum) * MAX_BUFSIZE(Variables))));
	}
}
PCoeffProcessingContext::~PCoeffProcessingContext() {
	std::cout << "Deleting input buffers..." << std::endl;
	size_t numFreedInputBuffers = 0;
	for(std::optional<NodeIndex*> buffer; (buffer = inputBufferReturnQueue.pop_wait()).has_value();) {
		free(buffer.value());
		numFreedInputBuffers++;
	}
	std::cout << "Deleted " << numFreedInputBuffers << " input buffers. " << std::endl;

	std::cout << "Deleting output buffers..." << std::endl;
	size_t numFreedOutputBuffers = 0;
	for(std::optional<ProcessedPCoeffSum*> buffer; (buffer = outputBufferReturnQueue.pop_wait()).has_value();) {
		free(buffer.value());
		numFreedOutputBuffers++;
	}
	std::cout << "Deleted " << numFreedOutputBuffers << " output buffers. " << std::endl;
}
