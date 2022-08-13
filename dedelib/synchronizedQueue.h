#pragma once

#include <cstdint>
#include <mutex>
#include <condition_variable>
#include <cassert>
#include <optional>
#include <memory>
#include <vector>

#include "slabAllocator.h"
#include "numaMem.h"


template<typename T>
class RingQueue {
	T* queueBuffer;
	T* queueBufferEnd;
	T* readHead;
	T* writeHead;

public:
	size_t capacity() const {
		return queueBufferEnd - queueBuffer;
	}
	bool empty() const {
		return readHead == writeHead;
	}
	size_t size() const {
		if(writeHead >= readHead) {
			return writeHead - readHead;
		} else {
			return (queueBufferEnd - readHead) + (writeHead - queueBuffer);
		}
	}

	RingQueue() noexcept : queueBuffer(nullptr), queueBufferEnd(nullptr), readHead(nullptr), writeHead(nullptr) {}
	RingQueue(size_t capacity) : 
		queueBuffer(new T[capacity + 1]), // Extra capacity margin so that writeHead != readHead when the buffer is full
		queueBufferEnd(this->queueBuffer + (capacity + 1)),
		readHead(this->queueBuffer),
		writeHead(this->queueBuffer) {}
	
	RingQueue(RingQueue&& other) noexcept : 
		queueBuffer(other.queueBuffer), 
		queueBufferEnd(other.queueBufferEnd), 
		readHead(other.readHead), 
		writeHead(other.writeHead) {
		
		other.queueBuffer = nullptr;
		other.queueBufferEnd = nullptr;
		other.readHead = nullptr;
		other.writeHead = nullptr;
	}

	RingQueue& operator=(RingQueue&& other) noexcept {
		std::swap(this->queueBuffer, other.queueBuffer);
		std::swap(this->queueBufferEnd, other.queueBufferEnd);
		std::swap(this->readHead, other.readHead);
		std::swap(this->writeHead, other.writeHead);
		return *this;
	}

	RingQueue(const RingQueue& other) : RingQueue(other.capacity()) {
		T* readPtrInOther = other.readHead;
		if(readPtrInOther > other.writeHead) { // loops around
			for(; readPtrInOther != other.queueBufferEnd; readPtrInOther++) {
				this->push(*readPtrInOther);
			}
			readPtrInOther = other.queueBuffer;
		}
		for(; readPtrInOther != other.writeHead; readPtrInOther++) {
			this->push(*readPtrInOther);
		}
	}

	RingQueue& operator=(const RingQueue& other) {
		this->~RingQueue();
		new(this) RingQueue(other);
		return *this;
	}

	~RingQueue() {
		delete[] queueBuffer;
	}

	void push(T item) {
		*writeHead++ = std::move(item);
		if(writeHead == queueBufferEnd) writeHead = queueBuffer;
		assert(writeHead != readHead);
	}

	T pop() {
		assert(readHead != writeHead);
		T result = std::move(*readHead++);
		if(readHead == queueBufferEnd) readHead = queueBuffer;
		return result;
	}
};

template<typename T>
class SynchronizedQueue {
	RingQueue<T> queue;
	std::mutex mutex;
	std::condition_variable readyForPop;
	bool isClosed = false;
public:
	SynchronizedQueue() = default;
	SynchronizedQueue(size_t capacity) :
		queue(capacity) {}

#ifndef NDEBUG
	~SynchronizedQueue() {assert(this->isClosed);}
#endif
	// Unprotected. Only use in single-thread context
	RingQueue<T>& get() {return queue;}
	const RingQueue<T>& get() const {return queue;}

	bool queueHasBeenClosed() {
		std::lock_guard<std::mutex> lock(mutex);
		return this->isClosed;
	}

	size_t size() {
		std::lock_guard<std::mutex> lock(mutex);
		return this->queue.size();
	}

	size_t capacity() const {
		return queue.capacity();
	}


	// Write side
	void push(T item) {
		{std::lock_guard<std::mutex> lock(mutex);
			assert(!this->isClosed);
			queue.push(std::move(item));
		}
		readyForPop.notify_one();
	}

	void pushN(const T* values, size_t count) {
		{std::lock_guard<std::mutex> lock(mutex);
			assert(!this->isClosed);
			for(size_t i = 0; i < count; i++) {
				queue.push(values[i]);
			}
		}
		readyForPop.notify_all();
	}

	void close() {
		{std::lock_guard<std::mutex> lock(mutex);
			isClosed = true;
		}
		readyForPop.notify_all();
	}

	// Read side
	std::optional<T> pop_wait() {
		std::unique_lock<std::mutex> lock(mutex);
		while(this->queue.empty() && !this->isClosed) {
			readyForPop.wait(lock);
		}

		if(!this->queue.empty()) {
			return std::make_optional(std::move(this->queue.pop()));
		} else {
			assert(this->isClosed);
			return std::nullopt;
		}
	}

	// Pops a number of elements into the provided buffer. 
	// May wait forever
	void popN_wait_forever(T* buffer, size_t numberToPop) {
		std::unique_lock<std::mutex> lock(mutex);
		while(this->queue.size() < numberToPop) {
			readyForPop.wait(lock);
		}
		for(size_t i = 0; i < numberToPop; i++) {
			buffer[i] = this->queue.pop();
		}
	}
};

template<typename T>
class SynchronizedStack {
	std::unique_ptr<T[]> stack;
	std::mutex mutex;
	std::condition_variable readyForPop;
public:
	size_t sz = 0;
	size_t cap;

	SynchronizedStack() = default;
	SynchronizedStack(size_t capacity) : stack(new T[capacity]), cap(capacity) {}

	// Unprotected. Only use in single-thread context
	T* get() {return stack.get();}
	const T* get() const {return stack.get();}

	T& operator[](size_t i) {return stack[i];}
	const T& operator[](size_t i) const {return stack[i];}

	size_t size() {
		std::lock_guard<std::mutex> lock(mutex);
		return sz;
	}

	size_t capacity() {
		std::lock_guard<std::mutex> lock(mutex);
		return cap;
	}

	void setCapacity(size_t newCapacity) {
		std::lock_guard<std::mutex> lock(mutex);

		std::unique_ptr<T[]> newStack(new T[newCapacity]);
		for(size_t i = 0; i < sz; i++) {
			newStack[i] = stack[i];
		}

		stack = std::move(newStack);
		cap = newCapacity;
	}

	// Write side
	void push(T item) {
		{std::lock_guard<std::mutex> lock(mutex);
			stack[sz++] = std::move(item);
		}
		readyForPop.notify_one();
	}

	void pushN(const T* values, size_t count) {
		{std::lock_guard<std::mutex> lock(mutex);
			for(size_t i = 0; i < count; i++) {
				stack[sz++] = values[i];
			}
		}
		readyForPop.notify_all();
	}

	// Read side
	// May wait forever
	T pop_wait() {
		std::unique_lock<std::mutex> lock(mutex);
		while(sz == 0) {
			readyForPop.wait(lock);
		}
		return stack[--sz];
	}

	// Pops a number of elements into the provided buffer. 
	// May wait forever
	void popN_wait(T* buffer, size_t numberToPop) {
		std::unique_lock<std::mutex> lock(mutex);
		while(sz < numberToPop) {
			readyForPop.wait(lock);
		}
		for(size_t i = 0; i < numberToPop; i++) {
			buffer[i] = std::move(this->stack[--sz]);
		}
	}
};


template<typename T>
class SynchronizedSlabAllocator {
	SlabAllocator<T> slabAlloc;

	std::mutex mutex;
	std::condition_variable readyForAlloc;
public:
	// Unprotected. Only use in single-thread context
	SlabAllocator<T>& get() {return slabAlloc;}
	const SlabAllocator<T>& get() const {return slabAlloc;}

	SynchronizedSlabAllocator() = default;
	SynchronizedSlabAllocator(size_t slabSize, size_t maxChunks, size_t align = alignof(T)) : slabAlloc(slabSize, maxChunks, align) {}

	T* alloc_wait(size_t allocSize) {
		std::unique_lock<std::mutex> lock(mutex);
		while(true) {
			T* buf = slabAlloc.alloc(allocSize);
			if(buf != nullptr) return buf;
			readyForAlloc.wait(lock);
		}
	}
	void free(const T* allocatedBuf) {
		{std::lock_guard<std::mutex> lock(mutex);
			slabAlloc.free(allocatedBuf);
		}
		readyForAlloc.notify_all();
	}
	void shrinkLastAllocation(uint64_t newSize) {
		{std::lock_guard<std::mutex> lock(mutex);
			slabAlloc.shrinkLastAllocation(newSize);
		}
	}
	bool owns(const T* allocatedBuf) const {
		return slabAlloc.owns(allocatedBuf);
	}
};


class SynchronizedIndexAllocator {
	SlabIndexAllocator alloc;

	std::mutex mutex;
	std::condition_variable readyForAlloc;
public:
	// Unprotected. Only use in single-thread context
	SlabIndexAllocator& get() {return alloc;}
	const SlabIndexAllocator& get() const {return alloc;}

	SynchronizedIndexAllocator() = default;
	SynchronizedIndexAllocator(size_t size) : alloc(size) {}

	size_t alloc_wait() {
		std::unique_lock<std::mutex> lock(mutex);
		while(true) {
			size_t result = alloc.alloc();
			if(result != INVALID_ALLOC) return result;
			readyForAlloc.wait(lock);
		}
	}
	void free(size_t idx) {
		{std::lock_guard<std::mutex> lock(mutex);
			alloc.free(idx);
		}
		readyForAlloc.notify_all();
	}
};

template<typename T>
class SynchronizedMultiQueue {
	std::mutex mutex;
	std::condition_variable readyForPop;
	bool isClosed = false;
public:
	std::vector<RingQueue<T>> queues;
	SynchronizedMultiQueue() = default;
	SynchronizedMultiQueue(size_t numQueues, size_t queueCapacity) {
		queues.reserve(numQueues);
		for(size_t i = 0; i < numQueues; i++) {
			queues.emplace_back(queueCapacity);
		}
	}

	void close() {
		if(isClosed) throw "Queue already closed!\n";
		{std::lock_guard<std::mutex> lock(mutex);
			isClosed = true;
		}
		readyForPop.notify_all();
	}

	// Write side
	void push(size_t queueIdx, T item) {
		{std::lock_guard<std::mutex> lock(mutex);
			queues[queueIdx].push(std::move(item));
		}
		readyForPop.notify_one();
	}

	void pushN(size_t queueIdx, const T* values, size_t count) {
		{std::lock_guard<std::mutex> lock(mutex);
			for(size_t i = 0; i < count; i++) {
				queues[queueIdx].push(values[i]);
			}
		}
		readyForPop.notify_all();
	}

	// Read side
	// May wait forever
	std::optional<T> pop_wait(size_t& lastQueueIdx) {
		std::unique_lock<std::mutex> lock(mutex);
		size_t curIdx = lastQueueIdx;

		while(true) {
			do {
				if(curIdx == 0) curIdx = this->queues.size();
				curIdx--;

				if(!this->queues[curIdx].empty()) {
					lastQueueIdx = curIdx;
					return this->queues[curIdx].pop();
				}
			} while(curIdx != lastQueueIdx);
			if(isClosed) return std::nullopt;
			readyForPop.wait(lock);
			// Try again
		}
	}
};

template<typename T>
class SynchronizedMultiNUMAAlloc {
public:
	std::unique_ptr<SynchronizedStack<T*>[]> queues;
	std::vector<std::pair<T*, T*>> slabs;
	SynchronizedMultiNUMAAlloc(size_t subSlabSize, size_t numSubSlabs, size_t numNUMADomains) : queues(new SynchronizedStack<T*>[numNUMADomains]), slabs() {
		slabs.reserve(numNUMADomains);

		for(size_t nn = 0; nn < numNUMADomains; nn++) {
			T* slab = numa_alloc_T<T>(subSlabSize * numSubSlabs, nn);
			slabs.push_back(std::make_pair(slab, slab + subSlabSize * numSubSlabs));
			queues[nn].setCapacity(numSubSlabs);
			for(size_t i = 0; i < numSubSlabs; i++) {
				queues[nn][i] = slab + subSlabSize * i;
				queues[nn].sz = numSubSlabs;
			}
		}
	}

	~SynchronizedMultiNUMAAlloc() {
		for(auto& slab : slabs) {
			numa_free(slab.first, (slab.second - slab.first) * sizeof(T));
		}
	}

	SynchronizedMultiNUMAAlloc(const SynchronizedMultiNUMAAlloc&) = delete;
	SynchronizedMultiNUMAAlloc(const SynchronizedMultiNUMAAlloc&&) = delete;
	SynchronizedMultiNUMAAlloc& operator=(const SynchronizedMultiNUMAAlloc&) = delete;
	SynchronizedMultiNUMAAlloc& operator=(const SynchronizedMultiNUMAAlloc&&) = delete;

	void allocN_wait(size_t numaNode, T** buffers, size_t count) {
		queues[numaNode].popN_wait(buffers, count);
	}

	void free(T* buf) {
		for(size_t nn = 0; nn < slabs.size(); nn++) {
			if(buf >= slabs[nn].first && buf < slabs[nn].second) {
				queues[nn].push(buf);
				return;
			}
		}
		throw "Could not find allocator for this buffer!";
	}
};


