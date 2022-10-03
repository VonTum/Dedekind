#include "threadPool.h"

#include <cassert>

// One thread is the calling thread
PThreadPool::PThreadPool(size_t numThreads) : threads(new pthread_t[numThreads-1]), numThreads(numThreads) {
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

PThreadPool::~PThreadPool() {
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
void PThreadPool::doInParallel(std::function<void()>&& work) {
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

void PThreadPool::doInParallel(std::function<void()>&& work, std::function<void()>&& mainThreadFunc) {
	funcToRun = std::move(work);
	std::unique_lock<std::mutex> selfLock(mtx); // locks mtx
	shouldStart = true;
	selfLock.unlock();
	threadStarter.notify_all();// all threads start running
	mainThreadFunc();
	funcToRun();
	selfLock.lock();
	shouldStart = false;
	threadsFinished.wait(selfLock, [this]() -> bool {return threadsWorking == 0; });
	selfLock.unlock();
}

void PThreadsSpread::join() {
	for(size_t threadI = 0; threadI < threadCount; threadI++) {
		pthread_join(threads[threadI], NULL);
	}
	this->threads = nullptr;
}
PThreadsSpread::~PThreadsSpread() {
	this->threads.get() == nullptr; // Must be joined before destroying
}
