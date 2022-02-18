#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <cassert>

template<typename T>
class SynchronizedQueue {
	std::queue<T> queue;
	std::mutex mutex;
	std::condition_variable readyForPop;
	bool isClosed = false;
public:
#ifndef NDEBUG
	~SynchronizedQueue() {assert(this->isClosed);}
#endif
	// Write side
	void push(T&& item) {
		{
			std::lock_guard<std::mutex> lock(mutex);
			assert(!this->isClosed);
			queue.push(std::move(item));
		}
		readyForPop.notify_one();
	}

	void push(const T& item) {
		{
			std::lock_guard<std::mutex> lock(mutex);
			assert(!this->isClosed);
			queue.push(item);
		}
		readyForPop.notify_one();
	}

	void close() {
		{
			std::lock_guard<std::mutex> lock(mutex);
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
			return std::optional<T>(this->queue.pop());
		} else {
			assert(this->isClosed);
			return std::optional<T>();
		}
	}
};
