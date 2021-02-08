#pragma once

#include "functionInputBitSet.h"
#include "aligned_alloc.h"

#include "dedekindDecomposition.h"

#include "knownData.h"

#include "iteratorFactory.h"

#include <thread>
#include <atomic>
#include <mutex>
#include <array>

#include <iostream>

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

struct LinkBufPtr {
	int offset;
	int size;
};

inline void skipLinkLayer(std::ifstream& file, size_t numElements, size_t numLinks) {
	file.ignore(numLinks * 4 + numElements);
}

inline void getLinkLayer(std::ifstream& file, int numElements, LinkBufPtr* offsetBuf, LinkedNode* linkBuf) {
	int offsetInLinkBuf = 0;
	for(int i = 0; i < numElements; i++) {
		uint8_t readBuf[MAX_EXPANSION * 4];
		file.read(reinterpret_cast<char*>(readBuf), 1); // reads one char
		unsigned int linkCount = readBuf[0];
		//std::cout << "found group linkCount: " << linkCount << "\n";
		file.read(reinterpret_cast<char*>(readBuf), static_cast<std::streamsize>(linkCount) * 4);

		offsetBuf[i].offset = offsetInLinkBuf;
		offsetBuf[i].size = linkCount;


		for(unsigned int i = 0; i < linkCount; i++) {
			linkBuf[offsetInLinkBuf].count = readBuf[i * 4];
			linkBuf[offsetInLinkBuf].index = int32_t(readBuf[i * 4 + 3]) | (int32_t(readBuf[i * 4 + 2]) << 8) | (int32_t(readBuf[i * 4 + 1]) << 16);
			//std::cout << "- count: " << linkBuf[offsetInLinkBuf].count  << " index: " << linkBuf[offsetInLinkBuf].index << "\n";
			offsetInLinkBuf++;
		}
	}
}

template<unsigned int Variables>
struct MBFDecomposition {
	struct Layer {
		LinkBufPtr* offsets;
		LinkedNode* fullBuf;
		FunctionInputBitSet<Variables>* fibs;
	};
	Layer* layers;

	constexpr static unsigned int LAYER_COUNT = (1 << Variables) + 1;

	MBFDecomposition() : layers(new Layer[LAYER_COUNT]) {
		for(size_t i = 0; i < LAYER_COUNT; i++) {
			layers[i].offsets = new LinkBufPtr[getLayerSize<Variables>(i)];
			layers[i].fullBuf = new LinkedNode[getLinkCount<Variables>(i)];
			layers[i].fibs = new FunctionInputBitSet<Variables>[getLayerSize<Variables>(i)];
		}
	}

	MBFDecomposition(MBFDecomposition&& other) : layers(other.layers) {
		other.layers = nullptr;
	}

	MBFDecomposition& operator=(MBFDecomposition&& other) {
		std::swap(this->layers, other.layers);
		return *this;
	}

	MBFDecomposition(const MBFDecomposition& other) = delete;
	MBFDecomposition& operator=(const MBFDecomposition& other) = delete;

	~MBFDecomposition() {
		if(layers != nullptr) {
			for(size_t i = 0; i < LAYER_COUNT; i++) {
				delete[] layers[i].offsets;
				delete[] layers[i].fullBuf;
				delete[] layers[i].fibs;
			}
			delete[] layers;
		}
	}

	IteratorFactory<LinkedNode*> iterLinksOf(int layerI, int nodeInLayer) const {
		const Layer& l = layers[layerI];

		LinkBufPtr& curOffset = l.offsets[nodeInLayer];
		LinkedNode* start = l.fullBuf + curOffset.offset;

		return IteratorFactory<LinkedNode*>{start, start + curOffset.size};
	}

	const FunctionInputBitSet<Variables>& get(int layerI, int nodeInLayer) const {
		return layers[layerI].fibs[nodeInLayer];
	}
};

template<unsigned int Variables>
MBFDecomposition<Variables> readFullMBFDecomposition() {
	std::string linkName = "mbfLinks";
	linkName.append(std::to_string(Variables));
	linkName.append(".mbfLinks");
	std::ifstream linkFile(linkName, std::ios::binary);

	assert(linkFile.is_open());

	MBFDecomposition<Variables> result;

	for(int i = 0; i < (1 << Variables); i++) {
		getLinkLayer(linkFile, getLayerSize<Variables>(i), result.layers[i].offsets, result.layers[i].fullBuf); // before layers
	}
	linkFile.close();

	std::string mbfName = "allUniqueMBFSorted";
	mbfName.append(std::to_string(Variables));
	mbfName.append(".mbf");
	std::ifstream mbfFile(mbfName, std::ios::binary);

	uint8_t buf[getMBFSizeInBytes<Variables>()];
	for(int layer = 0; layer < (1 << Variables); layer++) {
		for(int mbfI = 0; mbfI < getLayerSize<Variables>(layer); mbfI++) {
			mbfFile.read(reinterpret_cast<char*>(buf), getMBFSizeInBytes<Variables>());
			result.layers[layer].fibs[mbfI] = deserializeMBF<Variables>(buf);
		}
	}

	mbfFile.close();

	return result;
}


template<unsigned int Variables, typename T>
struct SwapperLayers {
	static constexpr size_t MAX_SIZE = getMaxLayerSize<Variables>();

	using IndexType = uint32_t;

	T* sourceCounts;
	T* destinationCounts;

	IndexType* dirtySourceList;
	IndexType* dirtyDestinationList;

	size_t dirtySourceListSize;
	size_t dirtyDestinationListSize;


	SwapperLayers() :
		sourceCounts(new T[MAX_SIZE]),
		destinationCounts(new T[MAX_SIZE]),
		dirtySourceList(new IndexType[MAX_SIZE]),
		dirtyDestinationList(new IndexType[MAX_SIZE]),
		dirtySourceListSize(0),
		dirtyDestinationListSize(0) {

		for(size_t i = 0; i < MAX_SIZE; i++) {
			sourceCounts[i] = 0;
			destinationCounts[i] = 0;
		}
	}

	~SwapperLayers() {
		delete[] sourceCounts;
		delete[] destinationCounts;
	}

	void pushNext() {
		std::swap(sourceCounts, destinationCounts);
		std::swap(dirtySourceList, dirtyDestinationList);
		std::swap(dirtySourceListSize, dirtyDestinationListSize);

		for(size_t i = 0; i < dirtyDestinationListSize; i++) {
			destinationCounts[dirtyDestinationList[i]] = 0;
		}

		dirtyDestinationListSize = 0;
	}

	IndexType* begin() { return this->dirtySourceList; }
	const IndexType* begin() const { return this->dirtySourceList; }
	IndexType* end() { return this->dirtySourceList + this->dirtySourceListSize; }
	const IndexType* end() const { return this->dirtySourceList + this->dirtySourceListSize; }

	T& operator[](IndexType index) { assert(index < MAX_SIZE); return sourceCounts[index]; }
	const T& operator[](IndexType index) const { assert(index < MAX_SIZE); return sourceCounts[index]; }
	
	void add(IndexType to, unsigned int count) {
		assert(to < MAX_SIZE);
		if(destinationCounts[to] == 0) {
			dirtyDestinationList[dirtyDestinationListSize++] = to;
			assert(dirtyDestinationListSize <= MAX_SIZE);
		}

		destinationCounts[to] += count;
	}
	void set(IndexType to) {
		assert(to < MAX_SIZE);
		if(destinationCounts[to] == 0) {
			dirtyDestinationList[dirtyDestinationListSize++] = to;
			assert(dirtyDestinationListSize <= MAX_SIZE);
		}

		destinationCounts[to] = true;
	}
};



template<unsigned int Variables>
std::pair<std::array<uint64_t, (1 << Variables)>, uint64_t> computeIntervalSize(int nodeLayer, int nodeIndex) {
	std::string linkName = "mbfLinks";
	linkName.append(std::to_string(Variables));
	linkName.append(".mbfLinks");
	std::ifstream linkFile(linkName, std::ios::binary);
	assert(linkFile.is_open());

	LinkBufPtr* offsetBuf = new LinkBufPtr[getMaxLayerSize<Variables>()];
	LinkedNode* linkBuf = new LinkedNode[getMaxLinkCount<Variables>()];

	for(int i = 0; i < nodeLayer; i++) {
		//getLinkLayer(linkFile, getLayerSize<Variables>(i), offsetBuf, linkBuf);
		skipLinkLayer(linkFile, getLayerSize<Variables>(i), getLinkCount<Variables>(i)); // before layers
	}

	//getLinkLayer(linkFile, getLayerSize<Variables>(nodeLayer-1), offsetBuf, linkBuf); // start layer

	SwapperLayers<Variables> swapper{};
	swapper.add(nodeIndex, 1);

	swapper.pushNext();

	std::array<uint64_t, (1 << Variables)> numberOfClassesPerLayer;
	for(uint64_t& item : numberOfClassesPerLayer) item = 0;
	numberOfClassesPerLayer[nodeLayer - 1] = 1;
	uint64_t intervalSize = 1;

	for(int i = nodeLayer; i < (1 << Variables); i++) {
		getLinkLayer(linkFile, getLayerSize<Variables>(i), offsetBuf, linkBuf); // downstream layers

		//std::cout << "checking layer " << i << "\n";

		for(int dirtyIndex : swapper) {
			int count = swapper[dirtyIndex];

			//std::cout << "  dirty index " << dirtyIndex << "\n";

			intervalSize += count;

			LinkBufPtr subLinkPtr = offsetBuf[dirtyIndex];

			for(int subLinkIdx = 0; subLinkIdx < subLinkPtr.size; subLinkIdx++) {
				LinkedNode ln = linkBuf[subLinkPtr.offset + subLinkIdx];
				//std::cout << "    subLink " << ln.count << "x" << ln.index << "\n";
				swapper.add(ln.index, ln.count * count);
			}
		}
		numberOfClassesPerLayer[i] = swapper.dirtyDestinationListSize;
		swapper.pushNext();
		/*for(int dirtyIndex : swapper) {
			int divideBy = i + 1 - nodeLayer;
			assert(swapper[dirtyIndex] % divideBy == 0);
			swapper[dirtyIndex] /= divideBy;
		}*/
	}

	linkFile.close();


	delete[] offsetBuf;
	delete[] linkBuf;

	return std::make_pair(numberOfClassesPerLayer, intervalSize);
}



