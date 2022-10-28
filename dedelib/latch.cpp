#include "latch.h"

Latch::Latch(int latchCount) noexcept : latchCount() {
	this->latchCount.store(latchCount);
}

void Latch::wait(std::unique_lock<std::mutex>& lock) noexcept {
	while(latchCount > 0) {
		starter.wait(lock);
	}
}

void Latch::notify(int amount) noexcept {
	int foundLatchCount = latchCount -= amount;

	if(foundLatchCount == 0) {
		starter.notify_all();
	}
}

void Latch::notify_wait(std::unique_lock<std::mutex>& lock, int amount) noexcept {
	int foundLatchCount = latchCount -= amount;

	if(foundLatchCount == 0) {
		starter.notify_all();
	} else {
		this->wait(lock);
	}
}

void Latch::notify_wait_unlock(std::unique_lock<std::mutex>& lock, int amount) noexcept {
	int foundLatchCount = latchCount -= amount;

	if(foundLatchCount == 0) {
		lock.unlock();
		starter.notify_all();
	} else {
		this->wait(lock);
		lock.unlock();
	}
}


void MutexLatch::wait() noexcept {
	std::unique_lock<std::mutex> lock(this->mutex);
	Latch::wait(lock);
}

void MutexLatch::notify(int amount) noexcept {
	Latch::notify(amount);
}

void MutexLatch::notify_wait(int amount) noexcept {
	std::unique_lock<std::mutex> lock(this->mutex);
	Latch::notify_wait_unlock(lock, amount);
}
