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
	PThreadPool(size_t numThreads);
	PThreadPool() : PThreadPool(std::thread::hardware_concurrency()) {}

	// cleanup
	~PThreadPool();

	// this work function may only return once all work has been completed
	void doInParallel(std::function<void()>&& work);

	// this work function may only return once all work has been completed
	void doInParallel(std::function<void()>&& work, std::function<void()>&& mainThreadFunc);
};

typedef PThreadPool ThreadPool;

class PThreadsSpread {
	std::unique_ptr<pthread_t[]> threads;
	size_t threadCount;
public:
	PThreadsSpread() : threads() {}
	PThreadsSpread(PThreadsSpread&&) = default;
	PThreadsSpread& operator=(PThreadsSpread&&) = default;
	PThreadsSpread(const PThreadsSpread&) = delete;
	PThreadsSpread& operator=(const PThreadsSpread&) = delete;
	template<typename T>
	PThreadsSpread(size_t threadCount, CPUAffinityType affinity, T* datas, void*(*func)(void*), size_t threadsPerData = 1) : 
		threads(new pthread_t[threadCount]), 
		threadCount(threadCount) {
		for(size_t i = 0; i < threadCount; i++) {
			size_t selectedDataIdx = i / threadsPerData;
			T* selectedData = &datas[selectedDataIdx];
			this->threads[i] = createPThreadAffinity(i, affinity, func, (void*) selectedData);
		}
	}
	void join();
	~PThreadsSpread();
};

