#pragma once

#include <thread>
#include <mutex>
#include <vector>

template<typename Iter, typename IterEnd, typename Func>
void finishIterInParallel(Iter iter, IterEnd iterEnd, Func funcToRun) {
#ifndef NO_MULTITHREAD
	unsigned int processorCount = std::thread::hardware_concurrency();
	if(processorCount > 1) {
		std::vector<std::thread> workers(processorCount - 1);

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
		std::vector<std::thread> workers(processorCount - 1);

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
// funcToRun = void(T& item, decltype(bufferProducer()))
// bufferProducer = Buffer()
template<typename Iter, typename IterEnd, typename ThreadTotal, typename Func, typename ThreadTotalMergeFunc>
ThreadTotal finishIterPartitionedWithSeparateTotals(Iter iter, IterEnd iterEnd, const ThreadTotal& initialPerThreadTotal, const Func& funcToRun, const ThreadTotalMergeFunc& totalMergeFunc) {
#ifndef NO_MULTITHREAD
	unsigned int processorCount = std::thread::hardware_concurrency();
	if(processorCount > 1) {
		std::vector<std::thread> workers(processorCount - 1);

		std::mutex iterMutex;

		ThreadTotal fullTotal = initialPerThreadTotal;
		std::mutex fullTotalMutex;

		auto work = [&iter, &iterEnd, &funcToRun, &iterMutex, &initialPerThreadTotal, &fullTotal, &fullTotalMutex, &totalMergeFunc]() {
			ThreadTotal selfTotal = initialPerThreadTotal;
			while(true) {
				iterMutex.lock();
				if(iter != iterEnd) {
					auto& item = *iter;
					++iter;
					iterMutex.unlock();

					funcToRun(item, selfTotal);
				} else {
					iterMutex.unlock();
					break;
				}
			}
			fullTotalMutex.lock();
			totalMergeFunc(fullTotal, selfTotal);
			fullTotalMutex.unlock();
		};

		for(std::thread& worker : workers) {
			worker = std::thread(work);
		}
		work();
		for(std::thread& worker : workers) {
			worker.join();
		}

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
