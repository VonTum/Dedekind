#pragma once

#include <thread>
#include <mutex>

#define NUMBER_OF_THREADS 12

template<typename Iter, typename IterEnd, typename Func>
void finishIterInParallel(Iter iter, IterEnd iterEnd, Func funcToRun) {
#if NUMBER_OF_THREADS > 1
	std::thread workers[NUMBER_OF_THREADS - 1];

	std::mutex iterMutex;

	auto work = [&iter, &iterEnd, &funcToRun, &iterMutex]() {
		while(true) {
			iterMutex.lock();
			if(iter != iterEnd) {
				auto& item = *iter;
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
#else
	for(; iter != iterEnd; ++iter) {
		funcToRun(*iter);
	}
#endif
}

template<typename Collection, typename Func>
void iterCollectionInParallel(const Collection& col, Func funcToRun) {
	finishIterInParallel(col.begin(), col.end(), std::move(funcToRun));
}
template<typename Collection, typename Func>
void iterCollectionInParallel(Collection& col, Func funcToRun) {
	finishIterInParallel(col.begin(), col.end(), std::move(funcToRun));
}


// expects two functions, one function for work, and another function for constructing the buffer that will be reused for the work:
// funcToRun = void(T& item, decltype(bufferProducer()))
// bufferProducer = Buffer()
template<typename Iter, typename IterEnd, typename BufferFunc, typename Func>
void finishIterInParallelWithPerThreadBuffer(Iter iter, IterEnd iterEnd, BufferFunc bufferProducer, Func funcToRun) {
#if NUMBER_OF_THREADS > 1
	std::thread workers[NUMBER_OF_THREADS - 1];

	std::mutex iterMutex;

	auto work = [&iter, &iterEnd, &funcToRun, &iterMutex, &bufferProducer]() {
		auto buffer = bufferProducer();
		while(true) {
			iterMutex.lock();
			if(iter != iterEnd) {
				auto& item = *iter;
				++iter;
				iterMutex.unlock();

				funcToRun(item, buffer);
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
#else
	auto buffer = bufferProducer();
	for(; iter != iterEnd; ++iter) {
		funcToRun(*iter, buffer);
	}
#endif
}

template<typename Collection, typename BufferFunc, typename Func>
void iterCollectionInParallelWithPerThreadBuffer(const Collection& col, BufferFunc bufferProducer, Func funcToRun) {
	finishIterInParallelWithPerThreadBuffer(col.begin(), col.end(), std::move(bufferProducer), std::move(funcToRun));
}
template<typename Collection, typename BufferFunc, typename Func>
void iterCollectionInParallelWithPerThreadBuffer(Collection& col, BufferFunc bufferProducer, Func funcToRun) {
	finishIterInParallelWithPerThreadBuffer(col.begin(), col.end(), std::move(bufferProducer), std::move(funcToRun));
}
