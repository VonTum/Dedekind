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
template<typename T>
void setQueueToBufferParts(SynchronizedQueue<T*>& target, T* bufMemory, size_t partSize, size_t numParts) {
	RingQueue<T*>& q = target.get();
	for(size_t i = 0; i < numParts; i++) {
		q.push(bufMemory + i * partSize);
	}
}

PCoeffProcessingContext::PCoeffProcessingContext(unsigned int Variables) : inputQueue(NUMA_SLICE_COUNT, NUM_INPUT_BUFFERS_PER_NODE) {
	std::cout 
		<< "Create PCoeffProcessingContext in 2 parts with " 
		<< Variables 
		<< " Variables, " 
		<< NUM_INPUT_BUFFERS_PER_NODE
		<< " buffers / socket, and "
		<< NUM_RESULT_BUFFERS_PER_NODE
		<< " result buffers\n" << std::flush;

	size_t alignedBufSize = getAlignedBufferSize(Variables);
	for(int socketI = 0; socketI < NUMA_SLICE_COUNT; socketI++) {
		this->numaInputMemory[socketI] = NUMAArray<NodeIndex>::alloc_onsocket(alignedBufSize * NUM_INPUT_BUFFERS_PER_NODE, socketI);
		this->numaResultMemory[socketI] = NUMAArray<ProcessedPCoeffSum>::alloc_onsocket(alignedBufSize * NUM_RESULT_BUFFERS_PER_NODE, socketI);
		this->numaQueues[socketI] = unique_numa_ptr<PCoeffProcessingContextEighth>::alloc_onnode(socketI * 4 + 3); // Prefer nodes 3 and 7 because that's where the FPGAs are

		setQueueToBufferParts(this->numaQueues[socketI]->inputBufferAlloc, this->numaInputMemory[socketI].buf, alignedBufSize, NUM_INPUT_BUFFERS_PER_NODE);
		setQueueToBufferParts(this->numaQueues[socketI]->resultBufferAlloc, this->numaResultMemory[socketI].buf, alignedBufSize, NUM_RESULT_BUFFERS_PER_NODE);
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
