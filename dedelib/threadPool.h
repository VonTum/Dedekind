#pragma once

#include <functional>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>

#include <pthread.h>
#include "threadUtils.h"

/*class ThreadPool {
	std::function<void()> funcToRun = []() {};
	std::vector<std::thread> threads{};

	// protects shouldStart, threadsWorking and their condition variables
	std::mutex mtx;

	// this tells the threads to start working
	std::condition_variable threadStarter;
	bool shouldStart = false;

	// this keeps track of the number of threads that are currently performing work, main thread may only return once all threads have finished working. 
	std::condition_variable threadsFinished;
	int threadsWorking = 0;

	// No explicit protection required since only the main thread may write to it and only in the destructor, so not when a new job is presented
	bool shouldExit = false;

public:
	ThreadPool() : threads(std::thread::hardware_concurrency() - 1) {
		for(std::thread& t : threads) {
			t = std::thread([this]() {
				std::unique_lock<std::mutex> selfLock(mtx); // locks mtx
				while(true) {
					threadStarter.wait(selfLock, [this]() -> bool {return shouldStart; }); // this unlocks the mutex. And relocks when exiting
					threadsWorking++;
					selfLock.unlock();

					if(shouldExit) break;
					funcToRun();

					selfLock.lock();
					shouldStart = false; // once any thread finishes we assume we've reached the end, keep all threads from 
					threadsWorking--;
					if(threadsWorking == 0) {
						threadsFinished.notify_one();
					}
				}
			});
		}
	}

	// cleanup
	~ThreadPool() {
		shouldExit = true;
		mtx.lock();
		shouldStart = true;
		mtx.unlock();
		threadStarter.notify_all();// all threads start running
		for(std::thread& t : threads) t.join(); // let threads exit
	}

	// this work function may only return once all work has been completed
	void doInParallel(std::function<void()>&& work) {
		funcToRun = std::move(work);
		std::unique_lock<std::mutex> selfLock(mtx); // locks mtx
		shouldStart = true;
		selfLock.unlock();
		threadStarter.notify_all();// all threads start running
		funcToRun();
		selfLock.lock();
		shouldStart = false;
		threadsFinished.wait(selfLock, [this]() -> bool {return threadsWorking == 0; });
		selfLock.unlock();
	}
};*/


class PThreadPool {
	std::function<void()> funcToRun = []() {};
	std::unique_ptr<pthread_t[]> threads;
	size_t numThreads;

	// protects shouldStart, threadsWorking and their condition variables
	std::mutex mtx;

	// this tells the threads to start working
	std::condition_variable threadStarter;
	bool shouldStart = false;

	// this keeps track of the number of threads that are currently performing work, main thread may only return once all threads have finished working. 
	std::condition_variable threadsFinished;
	int threadsWorking = 0;

	// No explicit protection required since only the main thread may write to it and only in the destructor, so not when a new job is presented
	bool shouldExit = false;

public:
	// One thread is the calling thread
	PThreadPool(size_t numThreads) : threads(new pthread_t[numThreads-1]), numThreads(numThreads) {
		auto threadFunc = [](void* thisVoid) -> void* {
			PThreadPool* self = (PThreadPool*) thisVoid;

			std::unique_lock<std::mutex> selfLock(self->mtx); // locks mtx
			while(true) {
				self->threadStarter.wait(selfLock, [self]() -> bool {return self->shouldStart; }); // this unlocks the mutex. And relocks when exiting
				self->threadsWorking++;
				selfLock.unlock();

				if(self->shouldExit) break;
				self->funcToRun();

				selfLock.lock();
				self->shouldStart = false; // once any thread finishes we assume we've reached the end, keep all threads from 
				self->threadsWorking--;
				if(self->threadsWorking == 0) {
					self->threadsFinished.notify_one();
				}
			}
			pthread_exit(nullptr);
			return nullptr;
		};

		for(size_t threadI = 0; threadI < numThreads-1; threadI++) {
			threads[threadI] = copyThreadAffinity(threadFunc, (void*) this);
		}
	}
	PThreadPool() : PThreadPool(std::thread::hardware_concurrency()) {}

	// cleanup
	~PThreadPool() {
		shouldExit = true;
		mtx.lock();
		shouldStart = true;
		mtx.unlock();
		threadStarter.notify_all();// all threads start running
		for(size_t threadI = 0; threadI < numThreads - 1; threadI++) {
			pthread_join(threads[threadI], nullptr); // let threads exit
		}
	}

	// this work function may only return once all work has been completed
	void doInParallel(std::function<void()>&& work) {
		funcToRun = std::move(work);
		std::unique_lock<std::mutex> selfLock(mtx); // locks mtx
		shouldStart = true;
		selfLock.unlock();
		threadStarter.notify_all();// all threads start running
		funcToRun();
		selfLock.lock();
		shouldStart = false;
		threadsFinished.wait(selfLock, [this]() -> bool {return threadsWorking == 0; });
		selfLock.unlock();
	}
};

typedef PThreadPool ThreadPool;
