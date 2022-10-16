#pragma once

#include <mutex>
#include <condition_variable>

class Latch {
	std::condition_variable starter;
	int latchCount;
public:
	Latch(int latchCount = 1) noexcept;
	void wait(std::unique_lock<std::mutex>& lock) noexcept;
	void notify(std::unique_lock<std::mutex>& lock, int amount = 1) noexcept;
	void notify_wait(std::unique_lock<std::mutex>& lock, int amount = 1) noexcept;
};

class MutexLatch : private Latch {
	std::mutex mutex;
public:
	using Latch::Latch;
	void wait() noexcept;
	void notify(int amount = 1) noexcept;
	void notify_wait(int amount = 1) noexcept;
};
