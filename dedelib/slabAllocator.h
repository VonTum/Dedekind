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

template<typename T>
class SlabAllocator {
	SlabAddrAllocator addrAllocator;
	T* slabStart;

public:
	SlabAllocator() noexcept : addrAllocator(), slabStart(nullptr) {}
	SlabAllocator(size_t slabSize, size_t maxChunks, size_t align = alignof(T)) : addrAllocator(alignUpTo(slabSize, align / sizeof(T)), maxChunks), slabStart(aligned_mallocT<T>(alignUpTo(slabSize, align / sizeof(T)), align)) {}
	~SlabAllocator() {
		aligned_free(slabStart);
	}

	SlabAllocator(const SlabAllocator&) = delete;
	SlabAllocator& operator=(const SlabAllocator&) = delete;
	SlabAllocator(SlabAllocator&& other) noexcept : addrAllocator(std::move(other.addrAllocator)), slabStart(other.slabStart) {other.slabStart = nullptr;}
	SlabAllocator& operator=(SlabAllocator&& other) noexcept {
		this->addrAllocator = std::move(other.addrAllocator);
		std::swap(this->slabStart, other.slabStart);
		return *this;
	}

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

template<typename T>
class SlabItemAllocator {
	bool* isAllocated;
	T* allocatedData;
	size_t size;
public:
	SlabItemAllocator() noexcept : isAllocated(nullptr), allocatedData(nullptr) {}
	SlabItemAllocator(size_t size) : isAllocated(new bool[size]), allocatedData(new T[size]), size(size) {
		for(size_t i = 0; i < size; i++) {
			isAllocated[i] = false;
		}
	}
	~SlabItemAllocator() noexcept {
		delete[] isAllocated;
		delete[] allocatedData;
	}

	SlabItemAllocator(const SlabItemAllocator&) = delete;
	SlabItemAllocator& operator=(const SlabItemAllocator&) = delete;
	SlabItemAllocator(SlabItemAllocator&& other) noexcept : isAllocated(other.isAllocated), allocatedData(other.allocatedData), size(other.size) {
		other.isAllocated(nullptr);
		other.allocatedData(nullptr);
	}
	SlabItemAllocator& operator=(SlabItemAllocator&& other) noexcept {
		std::swap(this->isAllocated, other.isAllocated);
		std::swap(this->allocatedData, other.allocatedData);
		std::swap(this->size, other.size);
		return *this;
	}

	T* alloc() noexcept {
		for(size_t i = 0; i < size; i++) {
			if(isAllocated[i] == false) {
				isAllocated[i] = true;
				return allocatedData + i;
			}
		}
		// unreachable
		assert(false);
	}
	void free(const T* v) noexcept {
		size_t idx = v - allocatedData;
		assert(idx < size);
		assert(isAllocated[idx]);
		isAllocated[idx] = false;
	}
};
