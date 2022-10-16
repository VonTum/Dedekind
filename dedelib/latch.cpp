#include "latch.h"

Latch::Latch(int latchCount) noexcept : latchCount(latchCount) {}

void Latch::wait(std::unique_lock<std::mutex>& lock) noexcept {
	while(latchCount > 0) {
		starter.wait(lock);
	}
}

void Latch::notify(std::unique_lock<std::mutex>& lock, int amount) noexcept {
	latchCount -= amount;

	if(latchCount == 0) {
		starter.notify_all();
	}
}

void Latch::notify_wait(std::unique_lock<std::mutex>& lock, int amount) noexcept {
	latchCount -= amount;

	if(latchCount == 0) {
		starter.notify_all();
	} else {
		this->wait(lock);
	}
}

void MutexLatch::wait() noexcept {
	std::unique_lock<std::mutex> lock(this->mutex);
	Latch::wait(lock);
}

void MutexLatch::notify(int amount) noexcept {
	std::unique_lock<std::mutex> lock(this->mutex);
	Latch::notify(lock, amount);
}

void MutexLatch::notify_wait(int amount) noexcept {
	std::unique_lock<std::mutex> lock(this->mutex);
	Latch::notify_wait(lock, amount);
}
