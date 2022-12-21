#include "processingContext.h"

#include <iostream>
#include <cassert>

#include "aligned_alloc.h"
#include "knownData.h"

#include "numaMem.h"

#include "flatBufferManagement.h"
#include "fileNames.h"
#include <string.h>

#define USE_NUMA_ALLOC_FOR_FPGA_BUFFERS


constexpr size_t NUM_INPUT_BUFFERS_PER_NODE = 120; // 240 buffers in total
constexpr size_t NUM_RESULT_BUFFERS_PER_NODE = 80; // 160 buffers in total

// Also alignment is required for openCL buffer sending and receiving methods
constexpr size_t ALLOC_ALIGN = 1 << 15;

static size_t getAlignedBufferSize(unsigned int Variables) {
	return alignUpTo(getMaxDeduplicatedBufferSize(Variables)+20, ALLOC_ALIGN);
	// Trust virtual memory. Only uses the max real buffer size of physical memory. 
	// Big alignment, to make sure the buffer alignment isn't the cause of the FPGA DMA issue alignment
	// return alignUpTo(mbfCounts[Variables] + ALLOC_ALIGN, ALLOC_ALIGN);
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

void PCoeffProcessingContextEighth::freeBuf(NodeIndex* bufToFree, size_t bufSize) {
	this->inputBufferAlloc.push(bufToFree);
}
void PCoeffProcessingContextEighth::freeBuf(ProcessedPCoeffSum* bufToFree, size_t bufSize) {
	memset(static_cast<void*>(bufToFree), 0xFF, sizeof(ProcessedPCoeffSum) * bufSize); // Fill with fixed data so any computation gaps are easily spotted
	this->resultBufferAlloc.push(bufToFree);
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

static void* posix_aligned_alloc(size_t size, size_t align) {
	void *result = NULL;
	int rc;
	rc = posix_memalign(&result, align, size);
	if(rc != 0) {
		perror("Error posix_memalign in PCoeffProcessingContext");
		std::abort();
	}
	return result;
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
#ifdef USE_NUMA_ALLOC_FOR_FPGA_BUFFERS
		this->numaInputMemory[socketI] = (NodeIndex*) numa_alloc_onsocket(alignedBufSize * NUM_INPUT_BUFFERS_PER_NODE * sizeof(NodeIndex), socketI);
		this->numaResultMemory[socketI] = (ProcessedPCoeffSum*) numa_alloc_onsocket(alignedBufSize * NUM_RESULT_BUFFERS_PER_NODE * sizeof(ProcessedPCoeffSum), socketI);
#else
		this->numaInputMemory[socketI] = (NodeIndex*) posix_aligned_alloc(alignedBufSize * NUM_INPUT_BUFFERS_PER_NODE * sizeof(NodeIndex), ALLOC_ALIGN * sizeof(NodeIndex));
		this->numaResultMemory[socketI] = (ProcessedPCoeffSum*) posix_aligned_alloc(alignedBufSize * NUM_RESULT_BUFFERS_PER_NODE * sizeof(ProcessedPCoeffSum), ALLOC_ALIGN * sizeof(ProcessedPCoeffSum));
#endif
		this->numaQueues[socketI] = unique_numa_ptr<PCoeffProcessingContextEighth>::alloc_onnode(socketI * 4 + 3); // Prefer nodes 3 and 7 because that's where the FPGAs are

		setQueueToBufferParts(this->numaQueues[socketI]->inputBufferAlloc, this->numaInputMemory[socketI], alignedBufSize, NUM_INPUT_BUFFERS_PER_NODE);
		setQueueToBufferParts(this->numaQueues[socketI]->resultBufferAlloc, this->numaResultMemory[socketI], alignedBufSize, NUM_RESULT_BUFFERS_PER_NODE);
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

	size_t alignedBufSize = getAlignedBufferSize(this->Variables);

	for(size_t socketI = 0; socketI < NUMA_SLICE_COUNT; socketI++) {
#ifdef USE_NUMA_ALLOC_FOR_FPGA_BUFFERS
		numa_free(this->numaInputMemory[socketI], alignedBufSize * NUM_INPUT_BUFFERS_PER_NODE * sizeof(NodeIndex));
		numa_free(this->numaResultMemory[socketI], alignedBufSize * NUM_RESULT_BUFFERS_PER_NODE * sizeof(ProcessedPCoeffSum));
#else
		free(this->numaInputMemory[socketI]);
		free(this->numaResultMemory[socketI]);
#endif
	}
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


template<size_t N, typename T>
static size_t getIndexOf(T* const (&arrs)[N], size_t bufSize, const T* ptr) {
	for(size_t i = 0; i < N; i++) {
		const T* bufStart = arrs[i];
		const T* bufEnd = bufStart + bufSize;
		if(ptr >= bufStart && ptr < bufEnd) {
			return i;
		}
	}
	// unreachable
	__builtin_unreachable();
	assert(false);
}

PCoeffProcessingContextEighth& PCoeffProcessingContext::getNUMAForBuf(const NodeIndex* id) const {
	size_t alignedBufSize = getAlignedBufferSize(this->Variables);
	size_t totalBufSize = alignedBufSize * NUM_INPUT_BUFFERS_PER_NODE;
	return *numaQueues[getIndexOf(numaInputMemory, totalBufSize, id)];
}
PCoeffProcessingContextEighth& PCoeffProcessingContext::getNUMAForBuf(const ProcessedPCoeffSum* id) const {
	size_t alignedBufSize = getAlignedBufferSize(this->Variables);
	size_t totalBufSize = alignedBufSize * NUM_RESULT_BUFFERS_PER_NODE;
	return *numaQueues[getIndexOf(numaResultMemory, totalBufSize, id)];
}
