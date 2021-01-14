#pragma once

#include "functionInputBitSet.h"
#include "aligned_alloc.h"

#include "dedekindDecomposition.h"

#include <thread>
#include <atomic>
#include <mutex>

#include <iostream>

constexpr size_t mbfCounts[]{2, 3, 5, 10, 30, 210, 16353, 490013148};
constexpr size_t MAX_EXPANSION = 40; // greater than 35, which is the max for 7, leave some leeway

// returns newMBFFoundCount
template<unsigned int Variables>
size_t findAllExpandedMBFsFast(const FunctionInputBitSet<Variables>& curMBF, FunctionInputBitSet<Variables>* expandedMBFs) {
	FunctionInputBitSet<Variables> newBits(andnot(curMBF.next().bitset, curMBF.bitset));

	size_t curSize = 0;

	newBits.forEachOne([&](size_t bits) {
		FunctionInputBitSet<Variables> newMBF = curMBF;
		newMBF.add(FunctionInput::underlyingType(bits));

		FunctionInputBitSet<Variables> canon = newMBF.canonize();

		for(size_t i = 0; i < curSize; i++) {
			// check for duplicates
			if(expandedMBFs[i] == canon) {
				return;
			}
		}
		// not a duplicate
		expandedMBFs[curSize++] = canon;
	});

	return curSize;
}

template<unsigned int Variables>
std::pair<BufferedSet<FunctionInputBitSet<Variables>>, size_t> generateAllMBFsFast() {
	BufferedSet<FunctionInputBitSet<Variables>> foundMBFs(mbfCounts[Variables] + mbfCounts[Variables] / 16); // some extra buffer room

	std::atomic<size_t> numberOfLinks(0);


	std::atomic<FunctionInputBitSet<Variables>*> nextToExpand(foundMBFs.begin());
	std::atomic<FunctionInputBitSet<Variables>*> knownMBFs(foundMBFs.begin());
	std::mutex newMBFMutex;

	FunctionInputBitSet<Variables> initialFibs = FunctionInputBitSet<Variables>::empty();
	foundMBFs.add(initialFibs);
	knownMBFs.fetch_add(1);

	std::cout << "Did predefined::  knownMBFs: " << (knownMBFs.load() - foundMBFs.begin()) << " nextToExpand: " << (nextToExpand.load() - foundMBFs.begin()) << "\n";

	int numberOfThreads = std::thread::hardware_concurrency();
	std::atomic<int> numberOfWaitingThreads(0);

	auto threadFunc = [&foundMBFs, &nextToExpand , &knownMBFs, &newMBFMutex, &numberOfWaitingThreads, &numberOfThreads, &numberOfLinks]() {
		while(true) {
			FunctionInputBitSet<Variables>* claimedNextToExpand = nextToExpand.load();
			tryAgain:
			FunctionInputBitSet<Variables>* upTo = knownMBFs.load();
			if(claimedNextToExpand < upTo) {
				bool wasClaimed = nextToExpand.compare_exchange_weak(claimedNextToExpand, claimedNextToExpand + 1);
				if(!wasClaimed) { // another thread edited inbetween, try again
					// claimedNextToExpand is updated by compare_exchange_weak to the changed value, it does not have to be loaded again
					goto tryAgain;
				} else {
					// claimedNextToExpand now points to a value claimed for processing by this thread, work on it now
					size_t index = claimedNextToExpand - foundMBFs.begin();
					if(index % 100000 == 0) {
						std::cout << index << "/" << mbfCounts[Variables] << "\n";
					}

					if(claimedNextToExpand->isFull()) {
						numberOfWaitingThreads.fetch_add(1);
						std::cout << "Done\n";
						return;
					}

					FunctionInputBitSet<Variables> expandedMBFs[MAX_EXPANSION];
					
					size_t newMBFFoundCount = findAllExpandedMBFsFast(*claimedNextToExpand, expandedMBFs);
					numberOfLinks.fetch_add(newMBFFoundCount);

					// remove some without having to lock, improves multithreading
					for(size_t i = 0; i < newMBFFoundCount; ) {
						//remove duplicates
						if(foundMBFs.contains(expandedMBFs[i])) {
							//std::cout << "removed " << expandedMBFs[i] << "\n";
							expandedMBFs[i] = expandedMBFs[--newMBFFoundCount];
						} else {
							i++;
						}
					}

					if(newMBFFoundCount > 0) {
						newMBFMutex.lock();
						for(size_t i = 0; i < newMBFFoundCount; i++) {
							if(foundMBFs.add(expandedMBFs[i])) {
								knownMBFs.fetch_add(1);
							}
						}
						newMBFMutex.unlock();
					}
				}
			} else {
				numberOfWaitingThreads.fetch_add(1);
				std::cout << "Went to sleep! " << numberOfWaitingThreads.load() << "/" << numberOfThreads << " sleeping\n";
				do {
					std::this_thread::sleep_for(std::chrono::milliseconds(50));
					if(numberOfWaitingThreads.load() == numberOfThreads) {
						// work done
						std::cout << "All threads waiting, Done!\n";
						return;
					}
				} while(knownMBFs.load() != upTo);
				// new work?
				numberOfWaitingThreads.fetch_add(-1);
				std::cout << "Woke up! " << numberOfWaitingThreads.load() << "/" << numberOfThreads << " sleeping\n";
			}
		}
	};

	std::thread* threads = new std::thread[numberOfThreads - 1];
	for(unsigned int i = 0; i < numberOfThreads - 1; i++) {
		threads[i] = std::thread(threadFunc);
	}

	threadFunc();

	for(unsigned int i = 0; i < numberOfThreads - 1; i++) {
		threads[i].join();
	}
	delete[] threads;

	return std::make_pair(std::move(foundMBFs), numberOfLinks.load());
}
