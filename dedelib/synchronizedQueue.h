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

enum class TryPopStatus {
	SUCCESS,
	EMPTY,
	CLOSED
};

template<typename T>
class SynchronizedQueue {
	RingQueue<T> queue;
	mutable std::mutex mutex;
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

	bool queueHasBeenClosed() const {
		std::lock_guard<std::mutex> lock(mutex);
		return this->isClosed;
	}

	size_t size() const {
		std::lock_guard<std::mutex> lock(mutex);
		return this->queue.size();
	}

	size_t capacity() const {
		return queue.capacity();
	}

	size_t freeSpace() const {
		std::lock_guard<std::mutex> lock(mutex);
		return queue.capacity() - this->queue.size();
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
	void popN_wait(T* buffer, size_t numberToPop) {
		std::unique_lock<std::mutex> lock(mutex);
		while(this->queue.size() < numberToPop) {
			readyForPop.wait(lock);
		}
		for(size_t i = 0; i < numberToPop; i++) {
			buffer[i] = this->queue.pop();
		}
	}

	TryPopStatus try_pop(T& out) {
		std::unique_lock<std::mutex> lock(mutex);
		if(!this->queue.empty()) {
			out = this->queue.pop();
			return TryPopStatus::SUCCESS;
		} else if(this->isClosed) {
			return TryPopStatus::CLOSED;
		} else {
			return TryPopStatus::EMPTY;
		}
	}
};

template<typename T>
class SynchronizedStack {
	std::unique_ptr<T[]> stack;
	mutable std::mutex mutex;
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

	size_t size() const {
		std::lock_guard<std::mutex> lock(mutex);
		return sz;
	}

	size_t capacity() const {
		std::lock_guard<std::mutex> lock(mutex);
		return cap;
	}

	size_t freeSpace() const {
		std::lock_guard<std::mutex> lock(mutex);
		return cap - sz;
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
public:
	bool isClosed = false;
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
	std::optional<T> pop_wait_rotate(size_t& lastQueueIdx) {
		std::unique_lock<std::mutex> lock(mutex);
		size_t curIdx = lastQueueIdx;

		while(true) {
			do {
				if(curIdx == 0) curIdx = this->queues.size();
				curIdx--;

				if(!this->queues[curIdx].empty()) {
					lastQueueIdx = curIdx; // update lastQueueIndex such that the next call will prefer a different queue
					return this->queues[curIdx].pop();
				}
			} while(curIdx != lastQueueIdx);
			if(isClosed) return std::nullopt;
			readyForPop.wait(lock);
			// Try again
		}
	}

	// Read side
	// May wait forever
	std::optional<T> pop_wait_prefer(size_t preferredQueue) {
		std::unique_lock<std::mutex> lock(mutex);
		size_t curIdx = preferredQueue;

		while(true) {
			do {
				if(!this->queues[curIdx].empty()) {
					return this->queues[curIdx].pop();
				}
				if(curIdx == 0) curIdx = this->queues.size();
				curIdx--;
			} while(curIdx != preferredQueue);
			if(isClosed) return std::nullopt;
			readyForPop.wait(lock);
			// Try again
		}
	}

	// Read side
	std::optional<T> pop_specific_wait(size_t queueIdx) {
		std::unique_lock<std::mutex> lock(mutex);

		while(queues[queueIdx].empty()) {
			if(isClosed) return std::nullopt;
			readyForPop.wait(lock);
			// Try again
		}
		return queues[queueIdx].pop();
	}

	// Read side
	// Return number of elements popped
	size_t popN_specific_wait(T* buf, size_t targetPopCount, size_t queueIdx) {
		RingQueue<T>& selectedQueue = this->queues[queueIdx];

		std::unique_lock<std::mutex> lock(mutex);

		while(selectedQueue.size() < targetPopCount) {
			size_t queueSize = selectedQueue.size();
			if(isClosed) {
				for(size_t i = 0; i < queueSize; i++) {
					buf[i] = selectedQueue.pop();
				}
				return queueSize;
			}
			readyForPop.wait(lock);
			// Try again
		}
		for(size_t i = 0; i < targetPopCount; i++) {
			buf[i] = selectedQueue.pop();
		}
		return targetPopCount;
	}
};

template<typename T>
class SynchronizedMultiNUMAAlloc {
public:
	std::unique_ptr<SynchronizedStack<T*>[]> queues;
	std::unique_ptr<std::pair<T*, T*>[]> slabs;
	size_t slabCount;
	SynchronizedMultiNUMAAlloc(size_t subSlabSize, size_t numSubSlabs, size_t numNUMADomains, size_t NUMAOffset, bool socketDomains = false) : queues(new SynchronizedStack<T*>[numNUMADomains]), slabs(new std::pair<T*, T*>[numNUMADomains]), slabCount(numNUMADomains) {
		size_t totalSlabSize = subSlabSize * numSubSlabs;
		for(size_t nn = 0; nn < numNUMADomains; nn++) {
			T* slab = socketDomains ? numa_alloc_socket_T<T>(totalSlabSize, nn) : numa_alloc_T<T>(totalSlabSize, nn);
			slabs[nn] = std::make_pair(slab, slab + totalSlabSize);
			queues[nn].setCapacity(numSubSlabs);
			for(size_t i = 0; i < numSubSlabs; i++) {
				queues[nn][i] = slab + subSlabSize * i;
				queues[nn].sz = numSubSlabs;
			}
		}
	}

	~SynchronizedMultiNUMAAlloc() {
		for(size_t i = 0; i < slabCount; i++) {
			std::pair<T*, T*>& slab = slabs[i];
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
		for(size_t nn = 0; nn < slabCount; nn++) {
			if(buf >= slabs[nn].first && buf < slabs[nn].second) {
				queues[nn].push(buf);
				return;
			}
		}
		throw "Could not find allocator for this buffer!";
	}
};

/*
Helper class to reduce contention on the mutex of a read queue, pops a batch of elements at a time instead of one by one
*/
template<typename T, size_t BatchSize>
class PopBatcher {
	T stored[BatchSize];
	size_t storedCount = 0;
public:
	/* Expects a function of the form size_t(T buf[BatchSize])
		It should return the number of elements popped, and write that number of elements to buf. 
		The Batch Popping function returning 0 elements mean the queue is closed, and then this function will also return nullopt
	*/
	template<typename BatchPopFunc>
	std::optional<T> pop_wait(BatchPopFunc& f) {
		if(storedCount == 0) {
			storedCount = f(stored); // Read a new batch
		}
		if(storedCount == 0) {
			return std::nullopt;
		} else {
			return std::move(stored[--storedCount]);
		}
	}
};
