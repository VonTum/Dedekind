#pragma once

#include <functional>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
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
	
	friend void* threadFunc(void*);
public:
	// One thread is the calling thread
	PThreadPool(size_t numThreads);
	PThreadPool(size_t numThreads, size_t startAtCPU);
	PThreadPool() : PThreadPool(std::thread::hardware_concurrency()) {}

	// cleanup
	~PThreadPool();

	// this work function may only return once all work has been completed
	void doInParallel(std::function<void()>&& work);

	// this work function may only return once all work has been completed
	void doInParallel(std::function<void()>&& work, std::function<void()>&& mainThreadFunc);

	// Expects a function of type void(size_t chunkStart, size_t chunkSize)
	template<typename Func>
	void iterRangeInParallel(size_t size, size_t chunkSize, const Func& work) {
		std::atomic<size_t> atomic_idx;
		atomic_idx.store(0);

		this->doInParallel([&](){
			while(true) {
				size_t blockStart = atomic_idx.fetch_add(chunkSize);
				if(blockStart >= size) {
					break;
				}
				size_t iterSize = chunkSize;
				if(blockStart + chunkSize > size) {
					iterSize = size - blockStart;
				}
				work(blockStart, iterSize);
			}
		});
	}
};

typedef PThreadPool ThreadPool;

class PThreadBundle {
	std::unique_ptr<pthread_t[]> threads;
	size_t threadCount;
public:
	PThreadBundle() : threads() {}
	PThreadBundle(pthread_t* threads, size_t threadCount) : threads(threads), threadCount(threadCount) {}
	PThreadBundle(PThreadBundle&&) = default;
	PThreadBundle& operator=(PThreadBundle&&) = default;
	PThreadBundle(const PThreadBundle&) = delete;
	PThreadBundle& operator=(const PThreadBundle&) = delete;
	void join();
	~PThreadBundle();
};

PThreadBundle multiThread(size_t threadCount, int cpuI, CPUAffinityType affinity, void* data, void*(*func)(void*));
PThreadBundle allCoresSpread(void* data, void*(*func)(void*));

template<typename T>
PThreadBundle spreadThreads(size_t threadCount, CPUAffinityType affinity, T* datas, void*(*func)(void*), size_t threadsPerData = 1) {
	pthread_t* threads = new pthread_t[threadCount]; 
	for(size_t i = 0; i < threadCount; i++) {
		size_t selectedDataIdx = i / threadsPerData;
		T* selectedData = &datas[selectedDataIdx];
		threads[i] = createPThreadAffinity(i, affinity, func, (void*) selectedData);
	}
	return PThreadBundle(threads, threadCount);
}

// Expects a function of the form void(int threadID)
template<typename Func>
void runInParallel(int threadCount, CPUAffinityType affinity, const Func& func) {
	struct ThreadData {
		int threadID;
		const Func* f;
		pthread_t thread;
	};
	ThreadData* datas = (ThreadData*) alloca(sizeof(ThreadData) * threadCount);

	for(int i = 0; i < threadCount; i++) {
		datas[i].threadID = i;
		datas[i].f = &func;
		datas[i].thread = createPThreadAffinity(i, affinity, [](void* voidData) -> void* {
			ThreadData* d = (ThreadData*) voidData;
			(*(d->f))(d->threadID);
			pthread_exit(NULL);
			return NULL;
		}, (void*) &datas[i]);
	}

	for(int i = 0; i < threadCount; i++) {
		void* ret = NULL;
		pthread_join(datas[i].thread, &ret);
	}
}

// Expects a function of the form void(int threadID)
template<typename Func>
void runInParallelOnAllCores(const Func& func) {
	runInParallel(std::thread::hardware_concurrency(), CPUAffinityType::CORE, func);
}

inline static int defaultInitFunc(int threadID) {
	return threadID;
}
/* 
	Expects a function of the form void(InitData& initData, IterT from, IterT to)
	By default InitData is int threadID
	Can also be extended with initialization data. Then requires separate InitData type

	Then should be used like this:
	InitData d = initFunc(int threadID);
	func(d, from, to)...;
*/
template<typename IterT, typename IntT, typename InitData = int, typename Func, typename InitFunc = decltype(defaultInitFunc)>
void iterRangeInParallelBlocks(int threadCount, CPUAffinityType affinity, IterT start, IterT end, IntT blockSize, const Func& func, const InitFunc& initFunc = defaultInitFunc) {
	std::atomic<IterT> idxAtomic;
	idxAtomic.store(start);

	struct ThreadData {
		int threadID;
		std::atomic<IterT>* idxAtomic;
		IterT idxEnd;
		IntT blockSize;
		const Func* f;
		const InitFunc* initFunc;
		pthread_t thread;
	};
	ThreadData* datas = (ThreadData*) alloca(sizeof(ThreadData) * threadCount);

	for(int i = 0; i < threadCount; i++) {
		datas[i].threadID = i;
		datas[i].idxAtomic = &idxAtomic;
		datas[i].idxEnd = end;
		datas[i].blockSize = blockSize;
		datas[i].f = &func;
		datas[i].initFunc = &initFunc;
		datas[i].thread = createPThreadAffinity(i, affinity, [](void* voidData) -> void* {
			ThreadData* d = (ThreadData*) voidData;
			int threadID = d->threadID;
			InitData initData = (*(d->initFunc))(threadID);
			const Func& func = *(d->f);
			std::atomic<IterT>& idxAtomic = *(d->idxAtomic);
			IterT end = d->idxEnd;
			IntT blockSize = d->blockSize;
			while(true) {
				IterT grabbedBlock = idxAtomic.fetch_add(blockSize);
				if(grabbedBlock >= end) {
					pthread_exit(NULL);
					return NULL;
				}
				IterT myEnd = grabbedBlock + blockSize;
				if(myEnd >= end) {
					myEnd = end;
				}
				func(initData, grabbedBlock, myEnd);
			}
		}, (void*) &datas[i]);
	}

	for(int i = 0; i < threadCount; i++) {
		void* ret = NULL;
		pthread_join(datas[i].thread, &ret);
	}
}

// Expects a function of the form void(InitData& initData, IterT curElem)
template<typename IterT, typename IntT, typename InitData = int, typename Func, typename InitFunc = decltype(defaultInitFunc)>
void iterRangeInParallelBlocksOnAllCores(IterT start, IterT end, IntT blockSize, const Func& func, const InitFunc& initFunc = defaultInitFunc) {
	iterRangeInParallelBlocks<IterT, IntT, InitData>(std::thread::hardware_concurrency(), CPUAffinityType::CORE, start, end, blockSize, [&func](InitData& initData, IterT from, IterT to){
		do {
			func(initData, from);
			++from;
		} while(from != to);
	}, initFunc);
}

// Used for debugging
// Expects a function of the form void(int threadID, IterT curElem)
template<typename IterT, typename IntT, typename InitData = int, typename Func, typename InitFunc = decltype(defaultInitFunc)>
void iterRangeInParallelBlocksOnOneCore(IterT start, IterT end, IntT blockSize, const Func& func, const InitFunc& initFunc = defaultInitFunc) {
	InitData initData = initFunc(0);
	for(; start != end; ++start) {
		func(initData, start);
	}
}
