#pragma once

#include "functionInputBitSet.h"
#include "aligned_alloc.h"

#include <thread>
#include <atomic>



constexpr size_t antiChainCounts[]{2, 3, 5, 10, 30, 210, 16353, 490013148};

template<unsigned int Variables>
std::pair<FunctionInputBitSet<Variables>*, FunctionInputBitSet<Variables>*> generateAllMBFs() {
	size_t maxSize = antiChainCounts[Variables];
	FunctionInputBitSet<Variables>* result = new FunctionInputBitSet<Variables>[maxSize + maxSize / 16]; // some extra buffer room

	std::atomic<FunctionInputBitSet<Variables>*> nextTo(result);
	std::atomic<FunctionInputBitSet<Variables>*> knownMBFs(result);

	{
		FunctionInputBitSet<Variables> fis = FunctionInputBitSet<Variables>::empty();
		fis.add(0);

		*nextTo.fetch_add(1) = fis;
		for(unsigned int v = 0; v < Variables; v++) {
			fis.add(1U << v);
			*nextTo.fetch_add(1) = fis;
		}
	}

	auto threadFunc = [&nextTo, &knownMBFs]() {
		
	};

	std::thread* threads = new std::thread[std::thread::hardware_concurrency() - 1];
	for(unsigned int i = 0; i < std::thread::hardware_concurrency() - 1; i++) {
		threads[i] = std::thread(threadFunc);
	}

	threadFunc();

	for(unsigned int i = 0; i < std::thread::hardware_concurrency() - 1; i++) {
		threads[i].join();
	}
	delete[] threads;
}
