#pragma once

#include <cstdint>
#include <cstddef>
#include <cassert>
#include <utility>

#include "aligned_alloc.h"

constexpr uint64_t INVALID_ALLOC = 0x8000000000000000;

class SlabAddrAllocator {
	uint64_t nextReserveAddr;
	// Starts of allocated blocks
	uint64_t* chunksInUse;
	uint64_t* chunksInUseEnd;
	uint64_t maxChunks;
public:
	uint64_t slabSize;

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
};

template<typename T, size_t Align = alignof(T)>
class SlabAllocator {
	SlabAddrAllocator addrAllocator;
	T* slabStart;

public:
	SlabAllocator() noexcept : addrAllocator(), slabStart(nullptr) {}
	SlabAllocator(size_t slabSize, size_t maxChunks) : addrAllocator(slabSize, maxChunks), slabStart(aligned_mallocT<T>(slabSize, Align)) {
		assert((slabSize * sizeof(T)) % Align == 0);
	}
	~SlabAllocator() {
		aligned_free(slabStart);
	}

	SlabAllocator(const SlabAllocator&) = delete;
	SlabAllocator& operator=(const SlabAllocator&) = delete;
	SlabAllocator(SlabAllocator&& other) noexcept : addrAllocator(move(other.addrAllocator)), slabStart(other.slabStart) {other.slabStart = nullptr;}
	SlabAllocator& operator=(SlabAllocator&& other) noexcept {
		this->addrAllocator = move(other.addrAllocator);
		std::swap(this->slabStart, other.slabStart);
	}

	T* alloc(size_t allocSize) {
		assert((allocSize * sizeof(T)) % Align == 0);
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
		assert((newSize * sizeof(T)) % Align == 0);
		addrAllocator.shrinkLastAllocation(newSize);
	}
	bool owns(const T* allocatedBuf) const {
		return allocatedBuf - slabStart < addrAllocator.slabSize;
	}
};
