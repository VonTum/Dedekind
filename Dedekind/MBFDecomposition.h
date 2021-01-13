#pragma once

#include "functionInputBitSet.h"
#include "aligned_alloc.h"

#include "dedekindDecomposition.h"

#include <thread>
#include <atomic>
#include <mutex>

template<typename IntType>
inline IntType nextMasked(IntType mbf, IntType mask) {
	// this works by abusing rollover when adding, OR with the mask produces all 1s until the next bit to be flipped, and 1s to cross any gaps
	IntType maskedResult = mbf | ~mask;
	maskedResult++;
	maskedResult &= mask;

	return maskedResult;
}

template<unsigned int Variables>
void findAllExpandedMBFs(const FunctionInputBitSet<Variables>& mbf, BufferedSet<FunctionInputBitSet<Variables>>& resultSet) {
	unsigned int nextLayerIdx = 0;
	for(; nextLayerIdx < Variables; nextLayerIdx++) {
		if(mbf.getLayer(nextLayerIdx).isEmpty()) break;
	}

	FunctionInputBitSet<Variables> topLayer = mbf.getLayer(nextLayerIdx - 1);
	FunctionInputBitSet<Variables> available = topLayer.next(); available.remove(0);//next enables 0, disable it for layer

	if constexpr(Variables <= 6) {
		FunctionInputBitSet<Variables> curExtention = FunctionInputBitSet<Variables>::empty();

		for(size_t i = 0; i < (size_t(1) << available.size()) - 1; i++) {
			curExtention.bitset.data = nextMasked(curExtention.bitset.data, available.bitset.data);

			FunctionInputBitSet<Variables> extendedMBF = mbf | curExtention;
			extendedMBF = extendedMBF.canonize();

			resultSet.add(extendedMBF);
		}
	} else if constexpr(Variables == 7) {
		uint64_t curExtention = 0;
		uint64_t availableMask = _mm_extract_epi64(available.bitset.data, 0) | _mm_extract_epi64(available.bitset.data, 1);

		for(size_t i = 0; i < (size_t(1) << available.size()) - 1; i++) {
			curExtention = nextMasked(curExtention, availableMask);

			FunctionInputBitSet<Variables> ext;
			ext.bitset.data = _mm_and_si128(available.bitset.data, _mm_set1_epi64x(curExtention));

			FunctionInputBitSet<Variables> extendedMBF = mbf | ext;
			extendedMBF = extendedMBF.canonize();

			resultSet.add(extendedMBF);
		}
	}
}

constexpr size_t mbfCounts[]{2, 3, 5, 10, 30, 210, 16353, 490013148};
constexpr size_t bufSizes[]{2, 3, 5, 10, 30, 210, 16353, 7000000};

template<unsigned int Variables>
std::pair<FunctionInputBitSet<Variables>*, FunctionInputBitSet<Variables>*> generateAllMBFs() {
	FunctionInputBitSet<Variables>* result = new FunctionInputBitSet<Variables>[mbfCounts[Variables] + mbfCounts[Variables] / 16]; // some extra buffer room

	std::atomic<FunctionInputBitSet<Variables>*> nextToExpand(result);
	std::atomic<FunctionInputBitSet<Variables>*> knownMBFs(result);
	std::mutex newMBFMutex;

	std::atomic<int> numberOfWaitingThreads(0);

	int numberOfThreads = std::thread::hardware_concurrency();

	{
		// initialize the buffer
		FunctionInputBitSet<Variables> fis = FunctionInputBitSet<Variables>::empty();
		*knownMBFs.fetch_add(1) = fis;
		nextToExpand.fetch_add(1);
		//fis.add(0);
		
		//nextToExpand.fetch_add(1);

		/*for(unsigned int v = 0; v < Variables; v++) {
			fis.add(1U << v);
			*knownMBFs.fetch_add(1) = fis;
		}*/

		DedekindDecomposition<NoExtraData> d(Variables);
		FunctionInputBitSet<Variables> lowerLayers = FunctionInputBitSet<Variables>::empty();
		for(unsigned int dLayer = 0; dLayer < Variables + 1; dLayer++) {
			lowerLayers |= FunctionInputBitSet<Variables>::layerMask(dLayer);
			assert(lowerLayers.isMonotonic());
			*knownMBFs.fetch_add(1) = lowerLayers;
			nextToExpand.fetch_add(1);
		}
		lowerLayers = FunctionInputBitSet<Variables>::layerMask(0);
		for(unsigned int dLayer = 1; dLayer < Variables; dLayer++) {
			LayerDecomposition<NoExtraData>& layer = d[dLayer];

			for(int subLayer = 1; subLayer < layer.getNumberOfFunctionInputs(); subLayer++) {
				for(BakedEquivalenceClass<EquivalenceClassInfo<NoExtraData>>& item : layer.subSizeMap(subLayer)) {
					FunctionInputBitSet<Variables> topPart = item.eqClass.functionInputSet;
					FunctionInputBitSet<Variables> newMBF = lowerLayers | topPart;
					assert(newMBF.isMonotonic());
					*knownMBFs.fetch_add(1) = newMBF;
				}
			}
			lowerLayers |= FunctionInputBitSet<Variables>::layerMask(dLayer);
		}
	}

	std::cout << "Did predefined::  knownMBFs: " << (knownMBFs.load() - result) << " nextToExpand: " << (nextToExpand.load() - result) << "\n";


	auto threadFunc = [&result, &nextToExpand, &knownMBFs, &newMBFMutex, &numberOfWaitingThreads, &numberOfThreads]() {
		BufferedSet<FunctionInputBitSet<Variables>> knownMBFCache(bufSizes[Variables]);

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

					//std::cout << "Claimed " << *claimedNextToExpand << " processing... ";

					size_t index = claimedNextToExpand - result;
					if(index % 10000 == 0) {
						std::cout << "index/" << mbfCounts[Variables] << "\n";
					}

					if(claimedNextToExpand->isFull()) {
						numberOfWaitingThreads.fetch_add(1);
						std::cout << "Done\n";
						return;
					}
					findAllExpandedMBFs(*claimedNextToExpand, knownMBFCache);

					//std::cout << " added " << knownMBFCache.size() << " new items\n";

					if(knownMBFCache.size() > 0) {
						std::sort(knownMBFCache.begin(), knownMBFCache.end(), [](typename BufferedSet<FunctionInputBitSet<Variables>>::SetNode& a, typename BufferedSet<FunctionInputBitSet<Variables>>::SetNode& b) -> bool {return a.key.size() < b.key.size(); });

						newMBFMutex.lock();
						FunctionInputBitSet<Variables>* buf = knownMBFs.load();
						for(auto& item : knownMBFCache) {
							*buf = item.key;
							//std::cout << "- " << item.item.key << "\n";
							buf++;
						}
						knownMBFs.store(buf);
						newMBFMutex.unlock();
						knownMBFCache.clear();
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

	return std::make_pair(result, nextToExpand.load());
}
