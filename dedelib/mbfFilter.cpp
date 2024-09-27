#include "mbfFilter.h"

#include <iostream>
#include <iomanip>
#include <memory>

#include "flatBufferManagement.h"
#include "fileNames.h"
#include "toString.h"
#include "timeTracker.h"
#include "simdTranspose.h"

void MBFFilterTree::debugPrintTree() {
    std::cout << std::fixed << std::setprecision(5);

    for(MBFFilterTreeNode node : this->tree) {
        #ifdef MAKE_STATISTICS
        for(int i = 0; i < node.depth; i++) {
            std::cout << "  ";
        }
        #endif
        if(node.bitIndex == 0) {
            std::cout << "[TERM]";
        } else {
            std::cout << '[' << node.bitIndex << " L" << __builtin_popcount(node.bitIndex) << ']';
        }
        #ifdef MAKE_STATISTICS
        std::cout
             << " mbf10%: " << node.validFraction
             << "   left: " << node.leftCount << " %" << node.leftFraction
             << "   right: " << node.rightCount << " %" << node.rightFraction << "\n";
        #endif
    }

    #ifdef MAKE_STATISTICS
    double totalPotentialComparisons = double(this->totalLeftMBFsInTree) * this->totalRightMBFsInTree;
    double validComparisonCountEstimate = 0.0;
    double unfilteredComparisonCountEstimate = 0.0;
    
    for(MBFFilterTreeNode node : this->tree) {
        if(node.bitIndex == 0 && node.leftCount != 0 && node.rightCount != 0) {
            validComparisonCountEstimate += node.validFraction * node.leftCount * node.rightCount;
            unfilteredComparisonCountEstimate += (1.0 - node.validFraction) * node.leftCount * node.rightCount;
        }
    }

    std::cout << std::setprecision(10);

    std::cout 
        << "\nTotal tree nodes: " << this->tree.size()
        << "\nTotal potential comparisons: " << totalPotentialComparisons
        << "\nEstimated valid comparison fraction: " << validComparisonCountEstimate / totalPotentialComparisons
        << "\nEstimated unfiltered comparison fraction: " << unfilteredComparisonCountEstimate / totalPotentialComparisons
        << std::endl;
    #endif
}


template<unsigned int Variables>
void testFilterTreePerformance() {
	size_t numRandomMBF = 8*1024*1024;
	Monotonic<Variables>* randomMBFBuf = const_cast<Monotonic<Variables>*>(readFlatBuffer<Monotonic<Variables>>(FileName::randomMBFs(Variables), numRandomMBF * sizeof(Monotonic<Variables>)));

	std::cout << "Loaded " << numRandomMBF << " MBF" << Variables << std::endl;

    std::cout << "Last Mbf: " << randomMBFBuf[numRandomMBF - 1] << std::endl;

    MBFFilterTree tree(randomMBFBuf, numRandomMBF);

    tree.debugPrintTree();
}



struct QuadraticCombinationAccumulator {
    uint64_t total = 0;
    uint64_t totalSearchSpace = 0;
    uint64_t totalCalls = 0;

    std::unique_ptr<uint32_t[]> temporaryBitsetTransposeBuffer{nullptr};
    size_t temporaryBitsetTransposeBufferSize = 0;
    std::vector<uint16_t> allBits;

    template<unsigned int Variables>
    void countValidCombinationsWithBitBuffer(
        Monotonic<Variables>* leftBuf, size_t leftSize,
        Monotonic<Variables>* rightBuf, size_t rightSize
    ) {
        constexpr size_t BITS_PER_MBF = 1 << Variables;
        constexpr size_t SIMD_BLOCK_SIZE_IN_BITS = 1024;
        constexpr size_t SIMD_BLOCK_SIZE_IN_BYTES = SIMD_BLOCK_SIZE_IN_BITS / 8;
        constexpr size_t U64_PER_BLOCK = SIMD_BLOCK_SIZE_IN_BYTES / sizeof(uint64_t);

        size_t numBlocksPerBit = (leftSize + SIMD_BLOCK_SIZE_IN_BITS - 1) / SIMD_BLOCK_SIZE_IN_BITS;

        size_t requiredBufferSize = numBlocksPerBit * BITS_PER_MBF * SIMD_BLOCK_SIZE_IN_BYTES;

        if(this->temporaryBitsetTransposeBufferSize < requiredBufferSize) {
            // Big blocks tend to be aligned
            temporaryBitsetTransposeBuffer = std::unique_ptr<uint32_t[]>{new uint32_t[requiredBufferSize / sizeof(uint32_t)]};
        }

        memset(this->temporaryBitsetTransposeBuffer.get(), 0, requiredBufferSize);

        transpose_per_32_bits(reinterpret_cast<uint64_t*>(leftBuf), BITS_PER_MBF / 64, leftSize, [&](uint32_t bits, size_t bitIndex, size_t blockIndex){
            size_t uint32_tPosition = bitIndex * numBlocksPerBit * SIMD_BLOCK_SIZE_IN_BYTES / sizeof(uint32_t) + blockIndex;
            assert(uint32_tPosition * sizeof(uint32_t) < requiredBufferSize);
            this->temporaryBitsetTransposeBuffer.get()[uint32_tPosition] = bits;
        });

        uint64_t thisTotal = 0;

        this->allBits.clear();
        for(size_t i = 0; i < rightSize; i++) {
            Monotonic<Variables> curRightMBF = rightBuf[i];

            AntiChain<Variables> topBits = curRightMBF.asAntiChain();
            topBits.forEachOne([&](size_t index){
                this->allBits.push_back(index);
            });

            allBits[allBits.size() - 1] |= 0x8000;
        }
        uint64_t curBlock[U64_PER_BLOCK];

        const uint64_t* bitsetBuffer = reinterpret_cast<uint64_t*>(this->temporaryBitsetTransposeBuffer.get());

        for(size_t blockI = 0; blockI < numBlocksPerBit; blockI++) {
            const uint64_t* curBlockBitsetBuf = bitsetBuffer + U64_PER_BLOCK * blockI;

            for(uint16_t bitIndex : this->allBits) {
                size_t bitOffset = U64_PER_BLOCK * numBlocksPerBit * (bitIndex & 0x7FFF);
                assert(bitOffset * sizeof(uint64_t) < requiredBufferSize);
                const uint64_t* thisBlock = curBlockBitsetBuf + bitOffset;

                for(size_t partInBlock = 0; partInBlock < U64_PER_BLOCK; partInBlock++) {
                    curBlock[partInBlock] &= thisBlock[partInBlock];
                }
                if(bitIndex & 0x8000) {
                    for(uint64_t elem : curBlock) {
                        thisTotal += popcnt64(elem);
                    }
                    for(size_t partInBlock = 0; partInBlock < U64_PER_BLOCK; partInBlock++) {
                        curBlock[partInBlock] = uint64_t(-1);
                    }
                }
            }
        }

        this->totalSearchSpace += leftSize * rightSize;
        this->total += thisTotal;
        this->totalCalls++;
    }

    template<unsigned int Variables>
    void countValidCombinationsQuadratic(
        Monotonic<Variables>* leftBuf, size_t leftSize,
        Monotonic<Variables>* rightBuf, size_t rightSize
    ) {
        uint64_t thisTotal = 0;
        for(size_t j = 0; j < rightSize; j++) {
            Monotonic<Variables> rightMbf = rightBuf[j];
            for(size_t i = 0; i < leftSize; i++) {
                Monotonic<Variables> leftMbf = leftBuf[i];
                if(rightMbf <= leftMbf) {
                    thisTotal++;
                }
            }
        }
        this->totalSearchSpace += leftSize * rightSize;
        this->total += thisTotal;
        this->totalCalls++;
    }

    void print() const {
        std::cout << "Total: " << this->total << "/" << totalSearchSpace << "=" << this->total * 100.0 / totalSearchSpace << "%" << std::endl;
        std::cout << "Calls made: " << this->totalCalls << std::endl;
    }
};

template<unsigned int Variables>
static void checkPartition(uint32_t filterBit, const Monotonic<Variables>* buf, size_t size, size_t zerosStartAt) {
    for(size_t i = 0; i < zerosStartAt; i++) {
        assert(buf[i].bf.bitset.get(filterBit) == true);
    }
    for(size_t i = zerosStartAt; i < size; i++) {
        assert(buf[i].bf.bitset.get(filterBit) == false);
    }
}

template<unsigned int Variables, typename RNG>
void countValidCombosWithFilterTreeRecurse(
    Monotonic<Variables>* leftBuf, size_t leftSize,
    Monotonic<Variables>* rightBuf, size_t rightSize,
    RNG& rng, QuadraticCombinationAccumulator& accumulator
) {
    // Figure out which bit it is best to split for
    size_t leftSampleSize = makeSample(leftBuf, leftSize, rng);
    size_t rightSampleSize = makeSample(rightBuf, rightSize, rng);

    // If it's still 0, then we didn't find a suitable split so we don't split
    uint32_t filterBit = findBestBitToSplitOver<uint8_t>(leftBuf, rightBuf, leftSampleSize, rightSampleSize, leftSize, rightSize);

    // Base case, if we didn't split. 
    if(filterBit == 0) {
        // No filter, terminal node, we just count combos

        accumulator.countValidCombinationsQuadratic(leftBuf, leftSize, rightBuf, rightSize);
    } else {
        // Go in recursion!
        size_t leftZerosStartAt = partitionByBit(filterBit, leftBuf, leftSize);
        size_t rightZerosStartAt = partitionByBit(filterBit, rightBuf, rightSize);

        checkPartition(filterBit, leftBuf, leftSize, leftZerosStartAt);
        checkPartition(filterBit, rightBuf, rightSize, rightZerosStartAt);


        //The idea is that of the combinations we can make, those with Left=0 and Right=1 will never be valid, and we scrap them immediately
        
        // Right MBFs with a 1 can only be matched to left MBFs that also have a 1
        countValidCombosWithFilterTreeRecurse(leftBuf, leftZerosStartAt, rightBuf, rightZerosStartAt, rng, accumulator);

        // Right = 0, then Left = 0 or 1
        countValidCombosWithFilterTreeRecurse(leftBuf, leftSize, rightBuf + rightZerosStartAt, rightSize - rightZerosStartAt, rng, accumulator);
    }
}

template<unsigned int Variables>
QuadraticCombinationAccumulator countValidCombosWithFilterTree(
    Monotonic<Variables>* leftBuf, size_t leftSize,
    Monotonic<Variables>* rightBuf, size_t rightSize
) {
    //std::mt19937_64 rng = properlySeededRNG();
    // Rng does not need to have insane properties to be good enough here
    // It's just for small shuffles
    std::default_random_engine rng;
    QuadraticCombinationAccumulator accumulator;

    countValidCombosWithFilterTreeRecurse(leftBuf, leftSize, rightBuf, rightSize, rng, accumulator);

    return accumulator;
}



template<unsigned int Variables>
void testTreeLessFilterTreePerformance() {
	size_t numRandomMBF = 256*1024;
	Monotonic<Variables>* randomMBFBuf = const_cast<Monotonic<Variables>*>(readFlatBuffer<Monotonic<Variables>>(FileName::randomMBFs(Variables), numRandomMBF * sizeof(Monotonic<Variables>)));

	std::cout << "Loaded " << numRandomMBF << " MBF" << Variables << std::endl;

    //std::cout << "Last Mbf: " << randomMBFBuf[numRandomMBF - 1] << std::endl;

    size_t singleBufSize = numRandomMBF / 2;
    Monotonic<Variables>* leftMBFs = randomMBFBuf;
    Monotonic<Variables>* rightMBFs = randomMBFBuf + singleBufSize;
    
    std::cout << "Between the two halves of these MBFs there are " << singleBufSize * singleBufSize << " possible combinations" << std::endl;
    {
        TimeTracker timer("Filter Tree ");
        QuadraticCombinationAccumulator accumulator = countValidCombosWithFilterTree(leftMBFs, singleBufSize, rightMBFs, singleBufSize);
        accumulator.print();
    }
    {
        TimeTracker timer("Quadratic ");
        QuadraticCombinationAccumulator accumulator;
        accumulator.countValidCombinationsQuadratic(leftMBFs, singleBufSize, rightMBFs, singleBufSize);
        accumulator.print();
    }
    {
        TimeTracker timer("Quadratic Fast ");
        QuadraticCombinationAccumulator accumulator;
        accumulator.countValidCombinationsWithBitBuffer(leftMBFs, singleBufSize, rightMBFs, singleBufSize);
        accumulator.print();
    }
}

template void testFilterTreePerformance<1>();
template void testFilterTreePerformance<2>();
template void testFilterTreePerformance<3>();
template void testFilterTreePerformance<4>();
template void testFilterTreePerformance<5>();
template void testFilterTreePerformance<6>();
template void testFilterTreePerformance<7>();
template void testFilterTreePerformance<8>();
template void testFilterTreePerformance<9>();

template void testTreeLessFilterTreePerformance<1>();
template void testTreeLessFilterTreePerformance<2>();
template void testTreeLessFilterTreePerformance<3>();
template void testTreeLessFilterTreePerformance<4>();
template void testTreeLessFilterTreePerformance<5>();
template void testTreeLessFilterTreePerformance<6>();
template void testTreeLessFilterTreePerformance<7>();
template void testTreeLessFilterTreePerformance<8>();
template void testTreeLessFilterTreePerformance<9>();
