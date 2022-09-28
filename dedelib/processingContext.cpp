#include "processingContext.h"

#include <iostream>
#include <cassert>

#include "aligned_alloc.h"
#include "knownData.h"

#include "numaMem.h"

// Try for at least 2MB huge pages
// Also alignment is required for openCL buffer sending and receiving methods
constexpr size_t ALLOC_ALIGN = 1 << 21;

static size_t getAlignedBufferSize(unsigned int Variables) {
	return alignUpTo(getMaxDeduplicatedBufferSize(Variables)+20, size_t(32) * 16);
}

PCoeffProcessingContextEighth::PCoeffProcessingContextEighth() : 
	inputBufferAlloc(NUM_INPUT_BUFFERS_PER_NODE),
	resultBufferAlloc(NUM_RESULT_BUFFERS_PER_NODE),
	outputQueue(std::min(NUM_INPUT_BUFFERS_PER_NODE, NUM_RESULT_BUFFERS_PER_NODE)),
	validationQueue(std::min(NUM_INPUT_BUFFERS_PER_NODE, NUM_RESULT_BUFFERS_PER_NODE)) {}

template<typename T>
void setStackToBufferParts(SynchronizedStack<T*>& target, T* bufMemory, size_t partSize, size_t numParts) {
	T** stack = target.get();
	for(size_t i = 0; i < numParts; i++) {
		stack[i] = bufMemory + i * partSize;
	}
	target.sz = numParts;
}

PCoeffProcessingContext::PCoeffProcessingContext(unsigned int Variables) : inputQueue(8, NUM_INPUT_BUFFERS_PER_NODE) {
	std::cout 
		<< "Create PCoeffProcessingContext in 8 parts with " 
		<< Variables 
		<< " Variables, " 
		<< NUM_INPUT_BUFFERS_PER_NODE * 8
		<< " buffers / NUMA node, and "
		<< NUM_RESULT_BUFFERS_PER_NODE * 8
		<< " result buffers\n" << std::flush;

	size_t alignedBufSize = getAlignedBufferSize(Variables);
	for(int numaNode = 0; numaNode < 8; numaNode++) {
		this->numaInputMemory[numaNode] = NUMAArray<NodeIndex>::alloc_onnode(alignedBufSize * NUM_INPUT_BUFFERS_PER_NODE, numaNode);
		this->numaResultMemory[numaNode] = NUMAArray<ProcessedPCoeffSum>::alloc_onnode(alignedBufSize * NUM_RESULT_BUFFERS_PER_NODE, numaNode);
		this->numaQueues[numaNode] = unique_numa_ptr<PCoeffProcessingContextEighth>::alloc_onnode(numaNode);

		setStackToBufferParts(this->numaQueues[numaNode]->inputBufferAlloc, this->numaInputMemory[numaNode].buf, alignedBufSize, NUM_INPUT_BUFFERS_PER_NODE);
		setStackToBufferParts(this->numaQueues[numaNode]->resultBufferAlloc, this->numaResultMemory[numaNode].buf, alignedBufSize, NUM_RESULT_BUFFERS_PER_NODE);
	}

	std::cout << "Finished PCoeffProcessingContext\n" << std::flush;
}

PCoeffProcessingContext::~PCoeffProcessingContext() {
	std::cout << "Destroy PCoeffProcessingContext, Deleting input and output buffers..." << std::endl;
}


template<typename T, size_t N>
size_t getIndexOf(const NUMAArray<T> (&arrs)[N], const T* ptr) {
	for(size_t i = 0; i < N; i++) {
		if(arrs[i].owns(ptr)) {
			return i;
		}
	}
	// unreachable
	assert(false);
}

PCoeffProcessingContextEighth& PCoeffProcessingContext::getNUMAForBuf(const NodeIndex* id) const {
	return *numaQueues[getIndexOf(numaInputMemory, id)];
}
PCoeffProcessingContextEighth& PCoeffProcessingContext::getNUMAForBuf(const ProcessedPCoeffSum* id) const {
	return *numaQueues[getIndexOf(numaResultMemory, id)];
}

void PCoeffProcessingContext::free(NodeIndex* inputBuf) const {
	this->getNUMAForBuf(inputBuf).inputBufferAlloc.push(inputBuf);
}
void PCoeffProcessingContext::free(ProcessedPCoeffSum* resultBuf) const {
	this->getNUMAForBuf(resultBuf).resultBufferAlloc.push(resultBuf);
}
