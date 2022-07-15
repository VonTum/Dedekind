#include "slabAllocator.h"

SlabAddrAllocator::SlabAddrAllocator() noexcept : chunksInUse(nullptr) {
	
}

SlabAddrAllocator::SlabAddrAllocator(uint64_t slabSize, size_t maxChunks) : 
	slabSize(slabSize), nextReserveAddr(0), chunksInUse(new uint64_t[maxChunks]), chunksInUseEnd(chunksInUse), maxChunks(maxChunks) {}

SlabAddrAllocator::~SlabAddrAllocator() {
	delete[] chunksInUse;
}

SlabAddrAllocator::SlabAddrAllocator(SlabAddrAllocator&& other) noexcept :
	slabSize(other.slabSize), 
	nextReserveAddr(other.nextReserveAddr), 
	chunksInUse(other.chunksInUse),
	chunksInUseEnd(other.chunksInUseEnd),
	maxChunks(other.maxChunks) {
	
	other.chunksInUse = nullptr;
}
SlabAddrAllocator& SlabAddrAllocator::operator=(SlabAddrAllocator&& other) noexcept {
	std::swap(this->nextReserveAddr, other.nextReserveAddr);
	std::swap(this->chunksInUse, other.chunksInUse);
	std::swap(this->chunksInUseEnd, other.chunksInUseEnd);
	std::swap(this->maxChunks, other.maxChunks);
	std::swap(this->slabSize, other.slabSize);

	return *this;
}

uint64_t SlabAddrAllocator::alloc(uint64_t allocSize) {
	assert(allocSize <= slabSize);
	if(chunksInUseEnd == chunksInUse + maxChunks) return INVALID_ALLOC;

	if(chunksInUseEnd == chunksInUse) { // Special case for no chunks
		chunksInUse[0] = 0;
		chunksInUseEnd = chunksInUse + 1;
		nextReserveAddr = allocSize;
		return 0;
	} else {
		uint64_t constrainingAlloc = chunksInUse[0];
		if(nextReserveAddr < constrainingAlloc) { // We're in a gap
			uint64_t spaceLeft = constrainingAlloc - nextReserveAddr;
			if(spaceLeft < allocSize) {
				return INVALID_ALLOC;
			}
		} else { // We're at the end!
			// Try to allocate at the end
			uint64_t spaceLeft = slabSize - nextReserveAddr;
			if(spaceLeft < allocSize) {
				// Can't? Try to restart from beginning
				if(constrainingAlloc >= allocSize) {
					nextReserveAddr = 0;
				} else {
					return INVALID_ALLOC;
				}
			}
		}
	}
	// All checks passed, we can just allocate it directly
	uint64_t allocated = nextReserveAddr;
	nextReserveAddr += allocSize;
	*chunksInUseEnd++ = allocated;
	return allocated;
}
void SlabAddrAllocator::free(uint64_t bufStart) {
	assert(bufStart <= slabSize);
	for(uint64_t* curChunk = chunksInUse; curChunk != chunksInUseEnd; curChunk++) {
		if(*curChunk == bufStart) {// Found! Remove from chunks list. 
			chunksInUseEnd--;
			for(;curChunk != chunksInUseEnd; curChunk++) {
				curChunk[0] = curChunk[1];
			}
			return;
		}
	}
	throw "Chunk not found!";
}

void SlabAddrAllocator::shrinkLastAllocation(uint64_t newSize) {
	uint64_t newChunkEnd = chunksInUseEnd[-1] + newSize;
	assert(newChunkEnd <= nextReserveAddr);
	nextReserveAddr = newChunkEnd;
}

size_t SlabAddrAllocator::getNumberOfChunksInUse() const {
	return chunksInUseEnd - chunksInUse;
}
