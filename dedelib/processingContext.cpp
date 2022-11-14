#include "processingContext.h"

#include <iostream>
#include <cassert>

#include "aligned_alloc.h"
#include "knownData.h"

#include "numaMem.h"

#include "flatBufferManagement.h"
#include "fileNames.h"
#include <string.h>

constexpr size_t NUM_INPUT_BUFFERS_PER_NODE = 160; // 320 buffers in total
constexpr size_t NUM_RESULT_BUFFERS_PER_NODE = 70; // 120 buffers in total

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

PCoeffProcessingContextEighth::~PCoeffProcessingContextEighth() {
	inputBufferAlloc.close();
	resultBufferAlloc.close();
}

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

PCoeffProcessingContext::PCoeffProcessingContext(unsigned int Variables) : Variables(Variables), inputQueue(NUMA_SLICE_COUNT, NUM_INPUT_BUFFERS_PER_NODE), topsAreReady(1), mbfs0Ready(1), mbfsBothReady(1) {
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

static void freeNUMA_MBFs(unsigned int Variables, const void* mbfs[2]) {
	size_t mbfSize = (1 << (Variables > 3 ? Variables-3 : 0)); // sizeof(Monotonic<Variables>)
	size_t mbfBufSize = mbfSize * mbfCounts[Variables];

	numa_free(const_cast<void*>(mbfs[0]), mbfBufSize);
	numa_free(const_cast<void*>(mbfs[1]), mbfBufSize);
}

PCoeffProcessingContext::~PCoeffProcessingContext() {
	std::cout << "Destroy PCoeffProcessingContext, Deleting input and output buffers..." << std::endl;
	freeNUMA_MBFs(this->Variables, this->mbfs);
}

void PCoeffProcessingContext::initTops(std::vector<JobTopInfo> tops) {
	this->tops = std::move(tops);
	this->topsAreReady.notify();
}

void PCoeffProcessingContext::initMBFS() {
	void* numaMBFBuffers[2];
	size_t mbfSize = (1 << (Variables > 3 ? Variables-3 : 0)); // sizeof(Monotonic<Variables>)
	size_t mbfBufSize = mbfSize * mbfCounts[Variables];
	allocSocketBuffers(mbfBufSize, numaMBFBuffers);
	readFlatVoidBufferNoMMAP(FileName::flatMBFs(Variables), mbfBufSize, numaMBFBuffers[0]);

	mbfs[0] = numaMBFBuffers[0];
	this->mbfs0Ready.notify();
	memcpy(numaMBFBuffers[1], numaMBFBuffers[0], mbfBufSize);
	mbfs[1] = numaMBFBuffers[1];
	this->mbfsBothReady.notify();
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