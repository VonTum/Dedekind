#pragma once

#include <thread>
#include <mutex>
#include <vector>

#define NO_MULTITHREAD

template<typename Func>
void runInParallel(const Func& work) {
	unsigned int processorCount = std::thread::hardware_concurrency();
	std::vector<std::thread> workers(processorCount - 1);
	for(std::thread& worker : workers) {
		worker = std::thread(work);
	}
	work();
	for(std::thread& worker : workers) {
		worker.join();
	}
}

template<typename Iter, typename IterEnd, typename Func, typename... Args>
void whileIterGrab(Iter& iter, const IterEnd& iterEnd, std::mutex& iterMutex, const Func& funcToRun, Args&... extraArgs) {
	while(true) {
		iterMutex.lock();
		if(iter != iterEnd) {
			auto& item = *iter;
			++iter;
			iterMutex.unlock();

			funcToRun(item, extraArgs...);
		} else {
			iterMutex.unlock();
			break;
		}
	}
}

template<typename Iter, typename IterEnd, typename Func>
void finishIterInParallel(Iter iter, IterEnd iterEnd, Func funcToRun) {
#ifndef NO_MULTITHREAD
	unsigned int processorCount = std::thread::hardware_concurrency();
	if(processorCount > 1) {
		std::mutex iterMutex;

		auto work = [&iter, &iterEnd, &iterMutex, &funcToRun]() {
			whileIterGrab(iter, iterEnd, iterMutex, funcToRun);
		};

		runInParallel(work);
	}
	else 
#endif
	{
		for(; iter != iterEnd; ++iter) {
			funcToRun(*iter);
		}
	}
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
#ifndef NO_MULTITHREAD
	unsigned int processorCount = std::thread::hardware_concurrency();
	if(processorCount > 1) {
		std::mutex iterMutex;

		auto work = [&]() {
			auto buffer = bufferProducer();
			whileIterGrab(iter, iterEnd, iterMutex, funcToRun, buffer);
		};

		runInParallel(work);
	} else
#endif
	{
		auto buffer = bufferProducer();
		for(; iter != iterEnd; ++iter) {
			funcToRun(*iter, buffer);
		}
	}
}

template<typename Collection, typename BufferFunc, typename Func>
void iterCollectionInParallelWithPerThreadBuffer(const Collection& col, BufferFunc bufferProducer, Func funcToRun) {
	finishIterInParallelWithPerThreadBuffer(col.begin(), col.end(), std::move(bufferProducer), std::move(funcToRun));
}
template<typename Collection, typename BufferFunc, typename Func>
void iterCollectionInParallelWithPerThreadBuffer(Collection& col, BufferFunc bufferProducer, Func funcToRun) {
	finishIterInParallelWithPerThreadBuffer(col.begin(), col.end(), std::move(bufferProducer), std::move(funcToRun));
}


// expects two functions, one function for work, and another function for constructing the buffer that will be reused for the work:
// funcToRun = void(T& item, ThreadTotal& localTotal)
// localTotal = initialTotal
template<typename Iter, typename IterEnd, typename ThreadTotal, typename Func, typename ThreadTotalMergeFunc>
ThreadTotal finishIterPartitionedWithSeparateTotals(Iter iter, IterEnd iterEnd, const ThreadTotal& initialPerThreadTotal, const Func& funcToRun, const ThreadTotalMergeFunc& totalMergeFunc) {
#ifndef NO_MULTITHREAD
	unsigned int processorCount = std::thread::hardware_concurrency();
	if(processorCount > 1) {
		std::mutex iterMutex;

		ThreadTotal fullTotal = initialPerThreadTotal;
		std::mutex fullTotalMutex;

		auto work = [&]() {
			ThreadTotal selfTotal = initialPerThreadTotal;
			whileIterGrab(iter, iterEnd, iterMutex, funcToRun, selfTotal);
			fullTotalMutex.lock();
			totalMergeFunc(fullTotal, selfTotal);
			fullTotalMutex.unlock();
		};

		runInParallel(work);

		return fullTotal;
	} else
#endif
	{
		ThreadTotal total = initialPerThreadTotal;
		for(; iter != iterEnd; ++iter) {
			funcToRun(*iter, total);
		}
		return total;
	}
}

template<typename Collection, typename ThreadTotal, typename Func, typename ThreadTotalMergeFunc>
ThreadTotal iterCollectionPartitionedWithSeparateTotals(const Collection& col, const ThreadTotal& initialPerThreadTotal, const Func& funcToRun, const ThreadTotalMergeFunc& totalMergeFunc) {
	return finishIterPartitionedWithSeparateTotals(col.begin(), col.end(), initialPerThreadTotal, funcToRun, totalMergeFunc);
}
template<typename Collection, typename ThreadTotal, typename Func, typename ThreadTotalMergeFunc>
ThreadTotal iterCollectionPartitionedWithSeparateTotals(Collection& col, const ThreadTotal& initialPerThreadTotal, const Func& funcToRun, const ThreadTotalMergeFunc& totalMergeFunc) {
	return finishIterPartitionedWithSeparateTotals(col.begin(), col.end(), initialPerThreadTotal, funcToRun, totalMergeFunc);
}


// expects two functions, one function for work, and another function for constructing the buffer that will be reused for the work:
// funcToRun = void(T& item, ThreadTotal& localTotal, decltype(bufferProducer())& localBuffer)
// localTotal = initialTotal
// bufProducer = Buffer()
template<typename Iter, typename IterEnd, typename ThreadTotal, typename Func, typename ThreadTotalMergeFunc, typename BufProducer>
ThreadTotal finishIterPartitionedWithSeparateTotalsWithBuffers(Iter iter, IterEnd iterEnd, const ThreadTotal& initialPerThreadTotal, const Func& funcToRun, const ThreadTotalMergeFunc& totalMergeFunc, const BufProducer& bufProducer) {
#ifndef NO_MULTITHREAD
	unsigned int processorCount = std::thread::hardware_concurrency();
	if(processorCount > 1) {
		std::mutex iterMutex;

		ThreadTotal fullTotal = initialPerThreadTotal;
		std::mutex fullTotalMutex;

		auto work = [&]() {
			auto buf = bufProducer();
			ThreadTotal selfTotal = initialPerThreadTotal;
			whileIterGrab(iter, iterEnd, iterMutex, funcToRun, selfTotal, buf);
			fullTotalMutex.lock();
			totalMergeFunc(fullTotal, selfTotal);
			fullTotalMutex.unlock();
		};

		runInParallel(work);

		return fullTotal;
	} else
#endif
	{
		auto buf = bufProducer();
		ThreadTotal total = initialPerThreadTotal;
		for(; iter != iterEnd; ++iter) {
			funcToRun(*iter, total, buf);
		}
		return total;
	}
}

template<typename Collection, typename ThreadTotal, typename Func, typename ThreadTotalMergeFunc, typename BufProducer>
ThreadTotal iterCollectionPartitionedWithSeparateTotalsWithBuffers(const Collection& col, const ThreadTotal& initialPerThreadTotal, const Func& funcToRun, const ThreadTotalMergeFunc& totalMergeFunc, const BufProducer& bufProducer) {
	return finishIterPartitionedWithSeparateTotalsWithBuffers(col.begin(), col.end(), initialPerThreadTotal, funcToRun, totalMergeFunc, bufProducer);
}
template<typename Collection, typename ThreadTotal, typename Func, typename ThreadTotalMergeFunc, typename BufProducer>
ThreadTotal iterCollectionPartitionedWithSeparateTotalsWithBuffers(Collection& col, const ThreadTotal& initialPerThreadTotal, const Func& funcToRun, const ThreadTotalMergeFunc& totalMergeFunc, const BufProducer& bufProducer) {
	return finishIterPartitionedWithSeparateTotalsWithBuffers(col.begin(), col.end(), initialPerThreadTotal, funcToRun, totalMergeFunc, bufProducer);
}

