#pragma once

#include <thread>
#include <mutex>

#define NUMBER_OF_THREADS 32

template<typename Iter, typename IterEnd, typename Func>
void finishIterInParallel(Iter iter, IterEnd iterEnd, Func funcToRun) {
	std::thread workers[NUMBER_OF_THREADS - 1];

	std::mutex iterMutex;

	auto work = [&iter, &iterEnd, &funcToRun, &iterMutex]() {
		while(true) {
			iterMutex.lock();
			if(iter != iterEnd) {
				auto item = *iter;
				++iter;
				iterMutex.unlock();

				funcToRun(item);
			} else {
				iterMutex.unlock();
				break;
			}
		}
	};

	for(std::thread& worker : workers) {
		worker = std::thread(work);
	}
	work();
	for(std::thread& worker : workers) {
		worker.join();
	}
}

template<typename Collection, typename Func>
void iterCollectionInParallel(const Collection& col, Func funcToRun) {
	finishIterInParallel(col.begin(), col.end(), std::move(funcToRun));
}
