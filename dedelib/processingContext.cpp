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

PCoeffProcessingContextEighth::PCoeffProcessingContextEighth(size_t numInputBuffers, size_t numResultBuffers) : 
	inputBufferAlloc(numInputBuffers),
	resultBufferAlloc(numResultBuffers),
	outputQueue(std::min(numInputBuffers, numResultBuffers)),
	validationQueue(std::min(numInputBuffers, numResultBuffers)) {}

template<typename T>
void setStackToBufferParts(SynchronizedStack<T*>& target, T* bufMemory, size_t partSize, size_t numParts) {
	T** stack = target.get();
	for(size_t i = 0; i < numParts; i++) {
		stack[i] = bufMemory + i * partSize;
	}
	target.sz = numParts;
}

PCoeffProcessingContext::PCoeffProcessingContext(unsigned int Variables, size_t numInputBuffers, size_t numResultBuffers) : inputQueue(8, numInputBuffers) {
	size_t alignedBufSize = getAlignedBufferSize(Variables);
	for(int numaNode = 0; numaNode < 8; numaNode++) {
		this->numaInputMemory[numaNode] = NUMAArray<NodeIndex>::alloc_onnode(alignedBufSize * numInputBuffers / 8, numaNode);
		this->numaResultMemory[numaNode] = NUMAArray<ProcessedPCoeffSum>::alloc_onnode(alignedBufSize * numResultBuffers / 8, numaNode);
		this->numaQueues[numaNode] = unique_numa_ptr<PCoeffProcessingContextEighth>::alloc_onnode(numaNode, numInputBuffers / 8, numResultBuffers / 8);

		setStackToBufferParts(this->numaQueues[numaNode]->inputBufferAlloc, this->numaInputMemory[numaNode].buf, alignedBufSize, numInputBuffers / 8);
		setStackToBufferParts(this->numaQueues[numaNode]->resultBufferAlloc, this->numaResultMemory[numaNode].buf, alignedBufSize, numResultBuffers / 8);
	}

	std::cout 
		<< "Create PCoeffProcessingContextHalf for with " 
		<< Variables 
		<< " Variables, " 
		<< numInputBuffers
		<< " buffers / NUMA node, and "
		<< numResultBuffers
		<< " result buffers" << std::endl;
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
