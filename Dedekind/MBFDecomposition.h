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
size_t findAllExpandedMBFsFast(const FunctionInputBitSet<Variables>& curMBF, std::pair<FunctionInputBitSet<Variables>, int>* expandedMBFs) {
	FunctionInputBitSet<Variables> newBits(andnot(curMBF.next().bitset, curMBF.bitset));

	size_t curSize = 0;

	newBits.forEachOne([&](size_t bits) {
		FunctionInputBitSet<Variables> newMBF = curMBF;
		newMBF.add(FunctionInput::underlyingType(bits));

		FunctionInputBitSet<Variables> canon = newMBF.canonize();

		for(size_t i = 0; i < curSize; i++) {
			// check for duplicates
			if(expandedMBFs[i].first == canon) {
				expandedMBFs[i].second++;
				return;
			}
		}
		// not a duplicate
		expandedMBFs[curSize].first = canon;
		expandedMBFs[curSize].second = 1;
		curSize++;
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

					std::pair<FunctionInputBitSet<Variables>, int> expandedMBFs[MAX_EXPANSION];
					
					size_t newMBFFoundCount = findAllExpandedMBFsFast(*claimedNextToExpand, expandedMBFs);
					numberOfLinks.fetch_add(newMBFFoundCount);

					// remove some without having to lock, improves multithreading
					for(size_t i = 0; i < newMBFFoundCount; ) {
						//remove duplicates
						if(foundMBFs.contains(expandedMBFs[i].first)) {
							//std::cout << "removed " << expandedMBFs[i] << "\n";
							expandedMBFs[i] = expandedMBFs[--newMBFFoundCount];
						} else {
							i++;
						}
					}

					if(newMBFFoundCount > 0) {
						newMBFMutex.lock();
						for(size_t i = 0; i < newMBFFoundCount; i++) {
							if(foundMBFs.add(expandedMBFs[i].first)) {
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

constexpr size_t layerSizes1[]{1, 1, 1};
constexpr size_t layerSizes2[]{1, 1, 1, 1, 1};
constexpr size_t layerSizes3[]{1, 1, 1, 1, 2, 1, 1, 1, 1};
constexpr size_t layerSizes4[]{1, 1, 1, 1, 2, 2, 2, 3, 4, 3, 2, 2, 2, 1, 1, 1, 1};
constexpr size_t layerSizes5[]{1, 1, 1, 1, 2, 2, 3, 4, 6, 7, 9, 10, 12, 12, 13, 13, 16, 13, 13, 12, 12, 10, 9, 7, 6, 4, 3, 2, 2, 1, 1, 1, 1};
constexpr size_t layerSizes6[]{1, 1, 1, 1, 2, 2, 3, 5, 7, 9, 14, 20, 29, 39, 53, 69, 93, 116, 146, 180, 225, 269, 328, 387, 459, 529, 611, 686, 769, 832, 892, 923, 951, 923, 892, 832, 769, 686, 611, 529, 459, 387, 328, 269, 225, 180, 146, 116, 93, 69, 53, 39, 29, 20, 14, 9, 7, 5, 3, 2, 2, 1, 1, 1, 1};
constexpr size_t layerSizes7[]{1, 1, 1, 1, 2, 2, 3, 5, 8, 10, 16, 25, 40, 62, 101, 156, 249, 385, 594, 899, 1367, 2036, 3032, 4468, 6571, 9572, 13933, 20131, 29014, 41556, 59306, 84099, 118719, 166406, 231794, 320296, 439068, 596093, 801197, 1064468, 1396828, 1807837, 2305705, 2894434, 3574182, 4338853, 5177869, 6075361, 7013439, 7971830, 8931651, 9874300, 10784604, 11648558, 12456475, 13199408, 13872321, 14470219, 14991439, 15433196, 15795759, 16077423, 16279195, 16399768, 16440466, 16399768, 16279195, 16077423, 15795759, 15433196, 14991439, 14470219, 13872321, 13199408, 12456475, 11648558, 10784604, 9874300, 8931651, 7971830, 7013439, 6075361, 5177869, 4338853, 3574182, 2894434, 2305705, 1807837, 1396828, 1064468, 801197, 596093, 439068, 320296, 231794, 166406, 118719, 84099, 59306, 41556, 29014, 20131, 13933, 9572, 6571, 4468, 3032, 2036, 1367, 899, 594, 385, 249, 156, 101, 62, 40, 25, 16, 10, 8, 5, 3, 2, 2, 1, 1, 1, 1};

constexpr size_t layerWidths[]{1, 1, 2, 3, 6, 10, 20, 35, 70};

template<unsigned int Variables>
constexpr size_t getLayerSize(size_t layer) {
	if constexpr(Variables == 1) {
		return layerSizes1[layer];
	} else if constexpr(Variables == 2) {
		return layerSizes2[layer];
	} else if constexpr(Variables == 3) {
		return layerSizes3[layer];
	} else if constexpr(Variables == 4) {
		return layerSizes4[layer];
	} else if constexpr(Variables == 5) {
		return layerSizes5[layer];
	} else if constexpr(Variables == 6) {
		return layerSizes6[layer];
	} else if constexpr(Variables == 7) {
		return layerSizes7[layer];
	} else {
		static_assert(Variables <= 7 && Variables != 0, "Not defined for >7");
	}
}

template<unsigned int Variables>
constexpr size_t getMaxLayerSize() {
	return getLayerSize<Variables>((1 << Variables) / 2);
}

struct LinkedNode {
	uint32_t count : 8; // max 35
	uint32_t index : 24; // max 16440466, which is just shy of 2^24
};

size_t serializeLinkedNodeList(const LinkedNode* ln, size_t count, uint8_t* outbuf) {
	*outbuf++ = static_cast<uint8_t>(count);
	for(size_t i = 0; i < count; i++) {
		*outbuf++ = static_cast<uint8_t>(ln[i].count);
		*outbuf++ = static_cast<uint8_t>(ln[i].index >> 16);
		*outbuf++ = static_cast<uint8_t>(ln[i].index >> 8);
		*outbuf++ = static_cast<uint8_t>(ln[i].index);
	}
	return count * 4 + 1;
}

template<unsigned int Variables>
void sortAndComputeLinks(std::ifstream& allClassesSorted, std::ofstream& outputMBFs, std::ofstream& linkNodeFile) {
	//FunctionInputBitSet<Variables>* buf = new FunctionInputBitSet<Variables>[getMaxLayerSize<Variables>()];

	BufferedSet<FunctionInputBitSet<Variables>> prevSet;
	for(size_t layer = 0; layer <= (1 << Variables); layer++) {
		std::cout << "Baking layer " << layer << "\n";

		size_t curLayerSize = getLayerSize<Variables>(layer);
		BufferedSet<FunctionInputBitSet<Variables>> fisSet(curLayerSize);

		for(size_t i = 0; i < curLayerSize; i++) {
			fisSet.add(deserializeMBF<Variables>(allClassesSorted));
		}

		// write to outputHashMaps
		for(const FunctionInputBitSet<Variables>& item : fisSet) {
			serializeMBF(item, outputMBFs);
		}
		
		if(layer != 0) { // skip first layer, nothing links to first layer
			std::cout << "Linking layer " << layer << " with " << (layer - 1) << "\n";
			for(const FunctionInputBitSet<Variables>& element : prevSet) {
				std::pair<FunctionInputBitSet<Variables>, int> expandedMBFBuf[MAX_EXPANSION];
				size_t foundNumber = findAllExpandedMBFsFast(element, expandedMBFBuf);

				LinkedNode linkedNodeBuf[MAX_EXPANSION];

				for(size_t i = 0; i < foundNumber; i++) {
					FunctionInputBitSet<Variables>* expanded = fisSet.find(expandedMBFBuf[i].first);
					linkedNodeBuf[i].count = expandedMBFBuf[i].second;
					linkedNodeBuf[i].index = expanded - fisSet.begin();
				}

				uint8_t lnBuf[MAX_EXPANSION * 5];
				size_t bytesCount = serializeLinkedNodeList(linkedNodeBuf, foundNumber, lnBuf);
				linkNodeFile.write(reinterpret_cast<char*>(lnBuf), bytesCount);
			}
		}

		prevSet = std::move(fisSet);
	}
}






