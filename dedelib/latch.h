#pragma once

#include <mutex>
#include <condition_variable>

class Latch {
	std::mutex mtx;
	std::condition_variable starter;
	int latchCount;
public:
	Latch(int latchCount = 1) : latchCount(latchCount) {}

	void wait() {
		std::unique_lock<std::mutex> lock(mtx);
		while(latchCount > 0) {
			starter.wait(lock);
		}
	}

	void notify() {
		std::unique_lock<std::mutex> lock(mtx);
		latchCount--;

		if(latchCount == 0) {
			starter.notify_all();
		}
	}
};

