#pragma once

#include <cstdint>
#include <mutex>
#include <condition_variable>
#include <cassert>
#include <optional>
#include <memory>

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

	void push(T&& item) {
		*writeHead++ = std::move(item);
		if(writeHead == queueBufferEnd) writeHead = queueBuffer;
		assert(writeHead != readHead);
	}

	void push(const T& item) {
		*writeHead++ = item;
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

	// Write side
	void push(T&& item) {
		{std::lock_guard<std::mutex> lock(mutex);
			assert(!this->isClosed);
			queue.push(std::move(item));
		}
		readyForPop.notify_one();
	}

	void push(const T& item) {
		{std::lock_guard<std::mutex> lock(mutex);
			assert(!this->isClosed);
			queue.push(item);
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
