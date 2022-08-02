#pragma once

#include <cstdint>
#include <cstddef>
#include <cassert>
#include <utility>

#include "aligned_alloc.h"

constexpr uint64_t INVALID_ALLOC = 0x8000000000000000;

class SlabAddrAllocator {
public:
	uint64_t slabSize;
private:
	uint64_t nextReserveAddr;
	// Starts of allocated blocks
	uint64_t* chunksInUse;
	uint64_t* chunksInUseEnd;
	uint64_t maxChunks;
public:

	SlabAddrAllocator() noexcept;
	SlabAddrAllocator(uint64_t slabSize, size_t maxChunks);
	~SlabAddrAllocator();

	SlabAddrAllocator(const SlabAddrAllocator&) = delete;
	SlabAddrAllocator& operator=(const SlabAddrAllocator&) = delete;
	SlabAddrAllocator(SlabAddrAllocator&&) noexcept;
	SlabAddrAllocator& operator=(SlabAddrAllocator&&) noexcept;

	uint64_t alloc(uint64_t allocSize);
	void free(uint64_t bufStart);
	void shrinkLastAllocation(uint64_t newSize);
	size_t getNumberOfChunksInUse() const;
};

/*
Does not clean up it's slab on destroy
*/
template<typename T>
class SlabAllocator {
	SlabAddrAllocator addrAllocator;
public:
	T* slabStart;
	
	SlabAllocator() noexcept : addrAllocator(), slabStart(nullptr) {}
	SlabAllocator(size_t slabSize, size_t maxChunks, T* slab, size_t align = alignof(T)) : 
		addrAllocator(alignUpTo(slabSize, align / sizeof(T)), maxChunks), slabStart(slab) {}
	SlabAllocator(size_t slabSize, size_t maxChunks, size_t align = alignof(T)) : 
		SlabAllocator(slabSize, maxChunks, aligned_mallocT<T>(alignUpTo(slabSize, align / sizeof(T)), align)) {}

	T* alloc(size_t allocSize) {
		//assert((allocSize * sizeof(T)) % Align == 0);
		size_t addr = addrAllocator.alloc(allocSize);
		if(addr != INVALID_ALLOC) {
			return slabStart + addr;
		} else {
			return nullptr;
		}
	}
	void free(const T* allocatedBuf) {
		addrAllocator.free(allocatedBuf - slabStart);
	}
	void shrinkLastAllocation(uint64_t newSize) {
		//assert((newSize * sizeof(T)) % Align == 0);
		addrAllocator.shrinkLastAllocation(newSize);
	}
	bool owns(const T* allocatedBuf) const {
		return allocatedBuf - slabStart < addrAllocator.slabSize;
	}
	size_t getNumberOfChunksInUse() const {
		return addrAllocator.getNumberOfChunksInUse();
	}
};


class SlabIndexAllocator {
	bool* isAllocated;
	size_t size;
public:
	SlabIndexAllocator() noexcept : isAllocated(nullptr) {}
	SlabIndexAllocator(size_t size) : isAllocated(new bool[size]), size(size) {
		for(size_t i = 0; i < size; i++) {
			isAllocated[i] = false;
		}
	}
	~SlabIndexAllocator() noexcept {
		delete[] isAllocated;
	}

	SlabIndexAllocator(const SlabIndexAllocator&) = delete;
	SlabIndexAllocator& operator=(const SlabIndexAllocator&) = delete;
	SlabIndexAllocator(SlabIndexAllocator&& other) noexcept : isAllocated(other.isAllocated), size(other.size) {
		other.isAllocated = nullptr;
	}
	SlabIndexAllocator& operator=(SlabIndexAllocator&& other) noexcept {
		std::swap(this->isAllocated, other.isAllocated);
		std::swap(this->size, other.size);
		return *this;
	}

	size_t alloc() noexcept {
		for(size_t i = 0; i < size; i++) {
			if(isAllocated[i] == false) {
				isAllocated[i] = true;
				return i;
			}
		}
		return INVALID_ALLOC;
	}
	void free(size_t idx) noexcept {
		assert(idx < size);
		assert(isAllocated[idx]);
		isAllocated[idx] = false;
	}
};

template<typename T>
class SlabItemAllocator {
	SlabIndexAllocator indexAlloc;
	T* allocatedData;
public:
	SlabItemAllocator() noexcept : indexAlloc(), allocatedData(nullptr) {}
	SlabItemAllocator(size_t size) : indexAlloc(size), allocatedData(new T[size]) {}
	~SlabItemAllocator() noexcept {
		delete[] allocatedData;
	}

	SlabItemAllocator(const SlabItemAllocator&) = delete;
	SlabItemAllocator& operator=(const SlabItemAllocator&) = delete;
	SlabItemAllocator(SlabItemAllocator&& other) noexcept : indexAlloc(std::move(other.indexAlloc)), allocatedData(other.allocatedData) {
		other.allocatedData = nullptr;
	}
	SlabItemAllocator& operator=(SlabItemAllocator&& other) noexcept {
		this->indexAlloc = std::move(other.indexAlloc);
		std::swap(this->size, other.size);
		return *this;
	}

	T* alloc() noexcept {
		size_t al = indexAlloc.alloc();
		if(al != INVALID_ALLOC) {
			return allocatedData + al;
		} else {
			return nullptr;
		}
	}
	void free(const T* v) noexcept {
		this->free(v - indexAlloc);
	}
};
