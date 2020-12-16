#pragma once

#include <shared_mutex>

class shared_lock_guard {
	std::shared_mutex& mutex;

public:
	inline shared_lock_guard(std::shared_mutex& mutex) : mutex(mutex) {
		mutex.lock_shared();
	}

	inline ~shared_lock_guard() {
		mutex.unlock_shared();
	}
};
