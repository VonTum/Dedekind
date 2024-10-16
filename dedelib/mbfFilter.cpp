#include "mbfFilter.h"

#include <iostream>
#include <iomanip>
#include <memory>
#include <stdint.h>
#include <vector>
#include <algorithm>
#include <random>
#include <string.h>

#include "flatBufferManagement.h"
#include "fileNames.h"
#include "toString.h"
#include "timeTracker.h"
#include "simdTranspose.h"
#include "funcTypes.h"
#include "generators.h"
#include "crossPlatformIntrinsics.h"

#define MAKE_STATISTICS

#define NOINLINE __attribute__ ((noinline))

struct MBFFilterTreeNode {
    // If bitIndex == 0, then this is an empty terminal node. Don't check, just follow bufIndex to a MBF buffer
    uint32_t bitIndex : 9;
    // If mbf[bitIndex] == 1, follow this index to the next node, otherwise go to this+1
    // If bitIndex == 0, then instead this instead is a terminal node. This index refers to the MBF buffer
    uint32_t branchTo : 23;

    #ifdef MAKE_STATISTICS
        int depth;
        // These fractions show what percentage of left & right MBFs arrived in this node
        double leftFraction;
        double rightFraction;
        size_t leftCount;
        size_t rightCount;
        // Estimate of how good the filtering is up to this point. Lower is better. 
        double validFraction;
    #endif
};

template<unsigned int Variables>
size_t countWhereBitIsSet(uint32_t bit, const Monotonic<Variables>* buf, size_t bufSize) {
    size_t total = 0;
    for(size_t i = 0; i < bufSize; i++) {
        uint64_t bitSelect = buf[i].bf.bitset.get64(bit) & 1;
        total += bitSelect;
    }
    return total;
}

template<unsigned int Variables>
size_t NOINLINE partitionByBit(uint32_t bit, Monotonic<Variables>* buf, size_t bufSize) {
    Monotonic<Variables>* midPoint = std::partition(buf, buf + bufSize, [bit](const Monotonic<Variables>& mbf){
        return mbf.bf.bitset.get(bit);
    });
    return midPoint - buf;
}

// Creates a smaller sample of the buffer to represent its behavior. 
// If the sample is too small, nothing happens. 
// If the sample is larger than MAX_SAMPLE_SIZE, we pick out MAX_SAMPLE_SIZE random elements
// from the whole buffer and move them to the front
template<typename T, typename RNG, size_t MAX_SAMPLE_SIZE = 255> // To fit count in uint8_t
size_t NOINLINE makeSample(T* buf, size_t size, RNG& rng) {
    if(size > MAX_SAMPLE_SIZE) {
        for(size_t i = 0; i < MAX_SAMPLE_SIZE; i++) {
            size_t selected = std::uniform_int_distribution<size_t>(i, size-1)(rng);
            if(i == selected) {continue;}
            T swapTmp = buf[i];
            buf[i] = buf[selected];
            buf[selected] = swapTmp;
        }
        return MAX_SAMPLE_SIZE;
    } else {
        return size;
    }
}

template<typename IntT, size_t TotalBits>
static void NOINLINE fastCountPerBit(const IntT *__restrict asBits, size_t numBitsets, IntT *__restrict counts) {
    constexpr size_t BitsPerPart = sizeof(IntT) * 8;
    constexpr size_t PartsPerMBF = TotalBits / BitsPerPart;
    constexpr size_t TotalBytes = TotalBits / 8;

    // Make sure our count can still contain 
    assert(numBitsets < (1 << BitsPerPart));
    
    #ifdef __AVX2__

    for(size_t i = 0; i < TotalBits; i++) {
        counts[i] = 0xDD;
    }
    constexpr size_t VectorRegBitSize = 256;
    constexpr size_t VectorRegByteSize = 32;
    if constexpr(TotalBits % VectorRegBitSize == 0 && sizeof(IntT) == 1) { // IntT == uint8_t
        __m256i firstBitsMask = _mm256_set1_epi8(1);

        for(size_t avxChunk = 0; avxChunk < TotalBytes; avxChunk += VectorRegByteSize) {
            __m256i subTotals[8] {
                _mm256_setzero_si256(),
                _mm256_setzero_si256(),
                _mm256_setzero_si256(),
                _mm256_setzero_si256(),
                _mm256_setzero_si256(),
                _mm256_setzero_si256(),
                _mm256_setzero_si256(),
                _mm256_setzero_si256(),
            };

            for(size_t curBitset = 0; curBitset < numBitsets; curBitset++) {
                __m256i curChunk = _mm256_load_si256(reinterpret_cast<const __m256i*>(asBits + TotalBytes * curBitset + avxChunk));

                // Unrolled loop because srli expects an immediate
                subTotals[0] = _mm256_add_epi8(subTotals[0], _mm256_and_si256(curChunk, firstBitsMask));
                subTotals[1] = _mm256_add_epi8(subTotals[1], _mm256_and_si256(_mm256_srli_epi16(curChunk, 1), firstBitsMask));
                subTotals[2] = _mm256_add_epi8(subTotals[2], _mm256_and_si256(_mm256_srli_epi16(curChunk, 2), firstBitsMask));
                subTotals[3] = _mm256_add_epi8(subTotals[3], _mm256_and_si256(_mm256_srli_epi16(curChunk, 3), firstBitsMask));
                subTotals[4] = _mm256_add_epi8(subTotals[4], _mm256_and_si256(_mm256_srli_epi16(curChunk, 4), firstBitsMask));
                subTotals[5] = _mm256_add_epi8(subTotals[5], _mm256_and_si256(_mm256_srli_epi16(curChunk, 5), firstBitsMask));
                subTotals[6] = _mm256_add_epi8(subTotals[6], _mm256_and_si256(_mm256_srli_epi16(curChunk, 6), firstBitsMask));
                subTotals[7] = _mm256_add_epi8(subTotals[7], _mm256_and_si256(_mm256_srli_epi16(curChunk, 7), firstBitsMask));
            }

            for(int curBit = 0; curBit < 8; curBit++) {
                _mm256_store_si256(reinterpret_cast<__m256i*>(counts + curBit * (TotalBits / 8) + avxChunk), subTotals[curBit]);
            }
        }
        return;
    }
    #endif

    for(size_t i = 0; i < TotalBits; i++) {
        counts[i] = 0;
    }

    // Only vectorizes well with clang
    // On second thought, even clang fucks this up. Hence manual vectorization up there
    for(size_t mbfIdx = 0; mbfIdx < numBitsets; mbfIdx++) {
        const IntT* curMBF = asBits + PartsPerMBF * mbfIdx;
        for(size_t bitInPart = 0; bitInPart < BitsPerPart; bitInPart++) {
            for(size_t partOfMBF = 0; partOfMBF < PartsPerMBF; partOfMBF++) {
                IntT justTheBit = (curMBF[partOfMBF] >> bitInPart) & 1U;
                counts[bitInPart * PartsPerMBF + partOfMBF] += justTheBit;
            }
        }
    }
}

template<typename IntT, unsigned int Variables>
static void NOINLINE checkFastCountPerBitBuffer(const Monotonic<Variables>* mbfs, size_t numMBFs, const IntT* counts) {
    constexpr size_t TotalBits = 1 << Variables;
    constexpr size_t BitsPerPart = sizeof(IntT) * 8;
    constexpr size_t PartsPerMBF = TotalBits / BitsPerPart;

    IntT newArray[TotalBits];
    for(size_t partOfMBF = 0; partOfMBF < PartsPerMBF; partOfMBF++) {
        for(size_t bitInPart = 0; bitInPart < BitsPerPart; bitInPart++) {
            size_t countIndex = bitInPart * PartsPerMBF + partOfMBF;
            size_t actualBit = partOfMBF * BitsPerPart + bitInPart;

            newArray[countIndex] = countWhereBitIsSet(actualBit, mbfs, numMBFs);
        }
    }

    bool anyFailure = false;
    for(size_t i = 0; i < 1 << Variables; i++) {
        std::cout << i << ": " << int(counts[i]) << " " << int(newArray[i]) << std::endl;
        if(counts[i] != newArray[i]) {anyFailure = true;}
    }
    assert(!anyFailure);
}

// How many comparisons we save it is worth to split the node for. 
//static uint64_t NOT_WORTH_IT_SPLIT_COUNT = 131072000; // See parameterStudyBitComparisons.txt
static uint64_t NOT_WORTH_IT_SPLIT_COUNT = 131072000; // See parameterStudyBitComparisons.txt

template<typename IntT, unsigned int Variables>
size_t NOINLINE findBestBitToSplitOver(
    const Monotonic<Variables>* leftMBFs, const Monotonic<Variables>* rightMBFs,
    size_t leftSampleSize, size_t rightSampleSize,
    size_t leftSize, size_t rightSize
) {
    constexpr size_t TotalBits = 1 << Variables;
    constexpr size_t BitsPerPart = sizeof(IntT) * 8;
    constexpr size_t PartsPerMBF = TotalBits / BitsPerPart;

    // This factor is to calculate the expected total amount of checks that could be avoided
    double sampleToTotalSizeFactor = static_cast<double>(leftSampleSize) * rightSampleSize / (leftSize * rightSize);
    
    uint64_t minimumSavingsForSplit = NOT_WORTH_IT_SPLIT_COUNT * sampleToTotalSizeFactor;

    const IntT* leftAsBits = reinterpret_cast<const IntT*>(leftMBFs);
    const IntT* rightAsBits = reinterpret_cast<const IntT*>(rightMBFs);

    alignas(64) IntT leftCounts[TotalBits];
    alignas(64) IntT rightCounts[TotalBits];

    fastCountPerBit<IntT, TotalBits>(leftAsBits, leftSampleSize, leftCounts);
    fastCountPerBit<IntT, TotalBits>(rightAsBits, rightSampleSize, rightCounts);

    //checkFastCountPerBitBuffer(leftMBFs, leftSampleSize, leftCounts);
    //checkFastCountPerBitBuffer(rightMBFs, rightSampleSize, rightCounts);

    size_t filterBit = 0; // If it's still 0, then we didn't find a suitable split so we don't split
    uint64_t bestFilterOutScore = minimumSavingsForSplit;

    for(size_t partOfMBF = 0; partOfMBF < PartsPerMBF; partOfMBF++) {
        for(size_t bitInPart = 0; bitInPart < BitsPerPart; bitInPart++) {
            size_t countIndex = bitInPart * PartsPerMBF + partOfMBF;
            size_t actualBit = partOfMBF * BitsPerPart + bitInPart;
            uint64_t filteredOutScore = (leftSampleSize - leftCounts[countIndex]) * rightCounts[countIndex];

            if(filteredOutScore > bestFilterOutScore) {
                filterBit = actualBit;
                bestFilterOutScore = filteredOutScore;
            }
        }
    }
    
    return filterBit;
    // Validation code
    /*{
        uint32_t slowFilterBit = 0; // If it's still 0, then we didn't find a suitable split so we don't split
        
        uint64_t bestFilterOutScore = minimumSavingsForSplit;
        // We don't cover 0 and 511 because they're kinda pointless. We don't have Top or Bottom in our sample
        for(uint32_t bit = 1; bit < (1 << Variables) - 1; bit++) {
            size_t leftOneCount = countWhereBitIsSet(bit, leftMBFs, leftSampleSize);
            size_t rightOneCount = countWhereBitIsSet(bit, rightMBFs, rightSampleSize);

            uint64_t filteredOutScore = (leftSampleSize - leftOneCount) * rightOneCount;

            if(filteredOutScore > bestFilterOutScore) {
                slowFilterBit = bit;
                bestFilterOutScore = filteredOutScore;
            }
        }

        size_t verifyLeftOneCount = countWhereBitIsSet(filterBit, leftMBFs, leftSampleSize);
        size_t verifyRightOneCount = countWhereBitIsSet(filterBit, rightMBFs, rightSampleSize);

        uint64_t verifyFilteredOutScore = (leftSampleSize - verifyLeftOneCount) * verifyRightOneCount;
        assert(bestFilterOutScore == verifyFilteredOutScore);
        assert(filterBit == slowFilterBit);
        return filterBit;
    }*/
}

class MBFFilterTree {
    std::vector<MBFFilterTreeNode> tree;
    uint32_t numberOfTerminalNodes = 0;
    
    size_t totalLeftMBFsInTree;
    size_t totalRightMBFsInTree;

    std::mt19937_64 rng;

    // Recursively split nodes and construct the tree. 
    template<unsigned int Variables>
    void constructTree(
        Monotonic<Variables>* leftBuf, size_t leftSize,
        Monotonic<Variables>* rightBuf, size_t rightSize,
        int depth
    ) {
        size_t newNodeIndex = this->tree.size();
        MBFFilterTreeNode& newNode = this->tree.emplace_back();

        // Figure out which bit it is best to split for
        size_t leftSampleSize = makeSample(leftBuf, leftSize, rng);
        size_t rightSampleSize = makeSample(rightBuf, rightSize, rng);

        #ifdef MAKE_STATISTICS
        {
            newNode.depth = depth;
            newNode.leftFraction = double(leftSize) / this->totalLeftMBFsInTree;
            newNode.rightFraction = double(rightSize) / this->totalRightMBFsInTree;
            newNode.leftCount = leftSize;
            newNode.rightCount = rightSize;
            size_t validCombos = 0;
            size_t comboCount = std::min(leftSampleSize, rightSampleSize);
            std::shuffle(leftBuf, leftBuf + comboCount, rng);
            for(size_t i = 0; i < comboCount; i++) {
                if(rightBuf[i] <= leftBuf[i]) {
                    validCombos++;
                }
            }
            newNode.validFraction = double(validCombos) / comboCount;
        }
        #endif

        // If it's still 0, then we didn't find a suitable split so we don't split
        uint32_t filterBit = findBestBitToSplitOver<uint8_t>(leftBuf, rightBuf, leftSampleSize, rightSampleSize, leftSize, rightSize);

        // Base case, if we didn't split. 
        newNode.bitIndex = filterBit;    
        if(filterBit == 0) {
            // No filter, terminal node
            newNode.branchTo = this->numberOfTerminalNodes++;
            return;
        }

        // Go in recursion!
        // This invalidates newNode!
        size_t leftZerosStartAt = partitionByBit(filterBit, leftBuf, leftSize);
        size_t rightZerosStartAt = partitionByBit(filterBit, rightBuf, rightSize);
        
        // First the 0 case. Because the tree is balanced like that
        // The zeros part of right is passed, but the whole of left is passed. 
        this->constructTree(leftBuf, leftSize, rightBuf + rightZerosStartAt, rightSize - rightZerosStartAt, depth + 1);
        this->tree[newNodeIndex].branchTo = this->tree.size(); // Points to the new node we're about to create

        // The ones part of right is passed, and only the left part that has ones is passed 
        this->constructTree(leftBuf, leftZerosStartAt, rightBuf, rightZerosStartAt, depth + 1);
    }

public:
    template<unsigned int Variables>
    MBFFilterTree(Monotonic<Variables>* mbfBuf, size_t bufSize) {
        size_t halfBufSize = bufSize / 2;
        
        this->totalLeftMBFsInTree = halfBufSize;
        this->totalRightMBFsInTree = halfBufSize;
        
        this->rng = properlySeededRNG();

        this->constructTree(mbfBuf, halfBufSize, mbfBuf + halfBufSize, halfBufSize, 0);
    }

    void debugPrintTree();
};



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

template<size_t NUM_POSSIBLE_BITS, size_t U64_PER_BLOCK>
uint64_t countValidCombinationsBitwiseSIMD(size_t numBlocksPerBit, const uint64_t *bitsetBuffer, std::vector<uint16_t>& allBits) {
    if(allBits.size() == 0) {return 0;}
    uint64_t thisTotal = 0;

#ifdef __AVX2__
    static_assert(U64_PER_BLOCK % 4 == 0);
    constexpr size_t VECTORS_PER_BLOCK = U64_PER_BLOCK / 4;
    #define SIMD_REG __m256i
    #define SIMD_SET_ONES _mm256_set1_epi8(0xFF)
    #define SIMD_POPCOUNT popcnt256
#elif defined(__AVX512F__)
    static_assert(U64_PER_BLOCK % 8 == 0);
    constexpr size_t VECTORS_PER_BLOCK = U64_PER_BLOCK / 8;
    #define SIMD_REG __m512i
    #define SIMD_SET_ONES _mm512_set1_epi8(0xFF)
    #define SIMD_POPCOUNT popcnt512
#else
    constexpr size_t VECTORS_PER_BLOCK = U64_PER_BLOCK;
    #define SIMD_REG uint64_t
    #define SIMD_SET_ONES uint64_t(-1)
    #define SIMD_POPCOUNT popcnt64
#endif

    SIMD_REG curBlock[VECTORS_PER_BLOCK];
    for (size_t partInBlock = 0; partInBlock < VECTORS_PER_BLOCK; partInBlock++) {
        curBlock[partInBlock] = SIMD_SET_ONES;
    }

    for (size_t blockI = 0; blockI < numBlocksPerBit; blockI++) {
        const uint64_t *curBlockBitsetBuf = bitsetBuffer + U64_PER_BLOCK * NUM_POSSIBLE_BITS * blockI;

        for(size_t curIdx = 0; curIdx < allBits.size();) {
            uint16_t bitIndex;
            // AND all bitsets together
            do {
                bitIndex = allBits[curIdx++];
                size_t bitOffset = U64_PER_BLOCK * (bitIndex & 0x7FFF);
                const SIMD_REG* thisBlock = reinterpret_cast<const SIMD_REG*>(curBlockBitsetBuf + bitOffset);

                for (size_t partInBlock = 0; partInBlock < VECTORS_PER_BLOCK; partInBlock++) {
                    curBlock[partInBlock] &= thisBlock[partInBlock];
                }
            } while(__builtin_expect((bitIndex & 0x8000) == 0, 1));

            // Then count how many 1's we have remaining
            // Optimized AVX version using clever shuffle trick appears not to be an optimization
            #if !defined(__AVX512F__) && defined(__AVX2__) && false
            const __m256i bitCountsPerNibble = _mm256_setr_epi8(
                0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
                0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4
            );
            __m256i count = _mm256_setzero_si256();  // Accumulator for popcount results

            // Iterate over the __m256i elements
            for (int i = 0; i < VECTORS_PER_BLOCK; i++) {
                // Mask lower 4 bits of each byte
                __m256i lower_nibbles = _mm256_and_si256(curBlock[i], _mm256_set1_epi8(0x0F));
                // Mask upper 4 bits of each byte
                __m256i upper_nibbles = _mm256_srli_epi16(curBlock[i], 4);
                upper_nibbles = _mm256_and_si256(upper_nibbles, _mm256_set1_epi8(0x0F));

                // Lookup the popcount for both lower and upper nibbles using shuffle
                __m256i popcount_lower = _mm256_shuffle_epi8(bitCountsPerNibble, lower_nibbles);
                __m256i popcount_upper = _mm256_shuffle_epi8(bitCountsPerNibble, upper_nibbles);

                // Sum the popcounts for the lower and upper nibbles
                __m256i popcount_sum = _mm256_add_epi8(popcount_lower, popcount_upper);

                // Accumulate the popcount result into the count variable
                count = _mm256_add_epi64(count, popcount_sum);
            }

            // Now that we have one 8-bit AVX register of results, add em all together
            // First, sum adjacent pairs of bytes using _mm256_sad_epu8 (absolute differences)
            __m256i sum1 = _mm256_sad_epu8(count, _mm256_setzero_si256());

            // sum1 now contains 4 lanes of 64-bit sums (each summing 8 adjacent bytes)
            // We need to further sum these 64-bit values
            uint64_t tmp[4];
            _mm256_storeu_si256((__m256i *)tmp, sum1);

            // Final sum of all bytes in the vector
            thisTotal += tmp[0] + tmp[1] + tmp[2] + tmp[3];
            #else
            for (SIMD_REG elem : curBlock) {
                thisTotal += SIMD_POPCOUNT(elem);
            }
            #endif
            for (size_t partInBlock = 0; partInBlock < VECTORS_PER_BLOCK; partInBlock++) {
                curBlock[partInBlock] = SIMD_SET_ONES;
            }
        }
    }

    return thisTotal;
}

struct QuadraticCombinationAccumulator {
    uint64_t total = 0;
    uint64_t checkedComparisons = 0;
    uint64_t totalCalls = 0;

    std::unique_ptr<uint32_t[]> temporaryBitsetTransposeBuffer{nullptr};
    size_t temporaryBitsetTransposeBufferSize = 0;
    std::vector<uint16_t> allBits;

    void reset() {
        this->total = 0;
        this->checkedComparisons = 0;
        this->totalCalls = 0;
    }

    template<unsigned int Variables>
    void NOINLINE countValidCombinationsWithBitBuffer(
        Monotonic<Variables>* leftBuf, size_t leftSize,
        Monotonic<Variables>* rightBuf, size_t rightSize
    ) {
        //std::cout << "leftSize: " << leftSize << "; rightSize: " << rightSize << std::endl;
        if(rightSize == 0) {
            return; // Add 0
        }
        constexpr size_t BITS_PER_MBF = 1 << Variables;
        constexpr size_t SIMD_BLOCK_SIZE_IN_BITS = 2048;
        constexpr size_t SIMD_BLOCK_SIZE_IN_BYTES = SIMD_BLOCK_SIZE_IN_BITS / 8;
        constexpr size_t U64_PER_BLOCK = SIMD_BLOCK_SIZE_IN_BYTES / sizeof(uint64_t);
        constexpr size_t U32_PER_BLOCK = SIMD_BLOCK_SIZE_IN_BYTES / sizeof(uint32_t);

        size_t numBlocksPerBit = (leftSize + SIMD_BLOCK_SIZE_IN_BITS - 1) / SIMD_BLOCK_SIZE_IN_BITS;

        size_t requiredBufferSize = numBlocksPerBit * BITS_PER_MBF * SIMD_BLOCK_SIZE_IN_BYTES;

        if(this->temporaryBitsetTransposeBufferSize < requiredBufferSize) {
            // Big blocks tend to be aligned
            this->temporaryBitsetTransposeBuffer = std::unique_ptr<uint32_t[]>{new uint32_t[requiredBufferSize / sizeof(uint32_t)]};
            this->temporaryBitsetTransposeBufferSize = requiredBufferSize;
        }

        memset(this->temporaryBitsetTransposeBuffer.get(), 0, requiredBufferSize);

        transpose_per_32_bits(reinterpret_cast<uint64_t*>(leftBuf), BITS_PER_MBF / 64, leftSize, [&](uint32_t bits, size_t bitIndex, size_t blockIndex){
            // A list of big bitsets
            //size_t position = bitIndex * numBlocksPerBit * U32_PER_BLOCK + blockIndex;

            // Array Of Array Of Interleaved Bit Chunks
            size_t thisChunk = blockIndex / U32_PER_BLOCK;
            size_t elementInChunk = blockIndex % U32_PER_BLOCK;
            size_t position = thisChunk * BITS_PER_MBF * U32_PER_BLOCK + bitIndex * U32_PER_BLOCK + elementInChunk;
            
            assert(position * sizeof(uint32_t) < requiredBufferSize);
            this->temporaryBitsetTransposeBuffer.get()[position] = bits;
        });

        this->allBits.clear();
        for(size_t i = 0; i < rightSize; i++) {
            Monotonic<Variables> curRightMBF = rightBuf[i];

            AntiChain<Variables> topBits = curRightMBF.asAntiChain();
            topBits.forEachOne([&](size_t index){
                this->allBits.push_back(index);
            });

            allBits[allBits.size() - 1] |= 0x8000;
        }

        const uint64_t *bitsetBuffer = reinterpret_cast<uint64_t *>(this->temporaryBitsetTransposeBuffer.get());

        this->checkedComparisons += leftSize * rightSize;
        this->totalCalls++;
        this->total += countValidCombinationsBitwiseSIMD<BITS_PER_MBF, U64_PER_BLOCK>(numBlocksPerBit, bitsetBuffer, this->allBits);
    }

    template<unsigned int Variables>
    void NOINLINE countValidCombinationsQuadratic(
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
        this->checkedComparisons += leftSize * rightSize;
        this->total += thisTotal;
        this->totalCalls++;
    }

    void print() const {
        std::cout << "Total: " << this->total << "/" << checkedComparisons << "=" << this->total * 100.0 / checkedComparisons << "%" << std::endl;
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

        accumulator.countValidCombinationsWithBitBuffer(leftBuf, leftSize, rightBuf, rightSize);
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
	size_t numRandomMBF = 1024*1024*16;
	Monotonic<Variables>* randomMBFBuf = const_cast<Monotonic<Variables>*>(readFlatBuffer<Monotonic<Variables>>(FileName::randomMBFs(Variables), numRandomMBF * sizeof(Monotonic<Variables>)));

	std::cout << "Loaded " << numRandomMBF << " MBF" << Variables << std::endl;

    //std::cout << "Last Mbf: " << randomMBFBuf[numRandomMBF - 1] << std::endl;

    /*size_t singleBufSize = numRandomMBF / 2;
    Monotonic<Variables>* leftMBFs = randomMBFBuf;
    Monotonic<Variables>* rightMBFs = randomMBFBuf + singleBufSize;
    
    std::cout << "Between the two halves of these MBFs there are " << singleBufSize * singleBufSize << " possible combinations" << std::endl;
    */

    /*QuadraticCombinationAccumulator accumulatorA;
    QuadraticCombinationAccumulator accumulatorB;
    for(size_t iter = 0; iter < 1000; iter++) {
        for(size_t alignSize = 0; alignSize < 1050; alignSize++) {
            accumulatorA.total = 0;
            accumulatorB.total = 0;
            accumulatorA.countValidCombinationsQuadratic(leftMBFs, alignSize, rightMBFs, 10);
            accumulatorB.countValidCombinationsWithBitBuffer(leftMBFs, alignSize, rightMBFs, 10);

            if(accumulatorA.total == accumulatorB.total) {
                //std::cout << alignSize << ": VALID: " << accumulatorA.total << std::endl;
            } else {
                std::cout << alignSize << ": WRONG: " << accumulatorA.total << " != " << accumulatorB.total << std::endl;
            }
        }
        leftMBFs += 2000;
        rightMBFs += 20;
    }*/

    
    size_t localSingleBufSize = 1024*128;
    while(localSingleBufSize <= numRandomMBF / 2) {
        Monotonic<Variables>* leftMBFs = randomMBFBuf;
        Monotonic<Variables>* rightMBFs = randomMBFBuf + localSingleBufSize;
        
        std::cout << "Single buffer size: " << localSingleBufSize << std::endl;
        std::cout << "Between the two halves of these MBFs there are " << localSingleBufSize * localSingleBufSize << " possible combinations" << std::endl;

        TimeTracker timer(std::string("Filter Tree for Size (") + std::to_string(localSingleBufSize) + ") ");
        QuadraticCombinationAccumulator accumulator = countValidCombosWithFilterTree(leftMBFs, localSingleBufSize, rightMBFs, localSingleBufSize);
        accumulator.print();
        localSingleBufSize *= 2;
    }



    /*{
        TimeTracker timer(std::string("Best Filter Tree (") + std::to_string(NOT_WORTH_IT_SPLIT_COUNT) + ") ");
        QuadraticCombinationAccumulator accumulator = countValidCombosWithFilterTree(leftMBFs, singleBufSize, rightMBFs, singleBufSize);
        accumulator.print();
    }
    return;

    {
        TimeTracker timer("Quadratic Fast ");
        QuadraticCombinationAccumulator accumulator;
        accumulator.countValidCombinationsWithBitBuffer(leftMBFs, singleBufSize, rightMBFs, singleBufSize);
        accumulator.print();
    }
    NOT_WORTH_IT_SPLIT_COUNT /= (1 << 15);
    for(int i = 0; i <= 30; i++) {
        TimeTracker timer(std::string("Filter Tree (") + std::to_string(NOT_WORTH_IT_SPLIT_COUNT) + ") ");
        QuadraticCombinationAccumulator accumulator = countValidCombosWithFilterTree(leftMBFs, singleBufSize, rightMBFs, singleBufSize);
        accumulator.print();
        NOT_WORTH_IT_SPLIT_COUNT *= 2;
    }*/
    /*{
        TimeTracker timer("Quadratic ");
        QuadraticCombinationAccumulator accumulator;
        accumulator.countValidCombinationsQuadratic(leftMBFs, singleBufSize, rightMBFs, singleBufSize);
        accumulator.print();
    }*/
}

template<unsigned int Variables>
uint64_t countWellKnownMBFCombinations(
    Monotonic<Variables>* leftBuf, size_t leftSize,
    Monotonic<Variables>* rightBuf, size_t rightSize
) {
    constexpr size_t MIDDLE_LAYER_OF_HIGHER_MBF = (Variables+1) / 2;

    Monotonic<Variables> middle0 = Monotonic<Variables>::getFilledUpToIncludingLayer(MIDDLE_LAYER_OF_HIGHER_MBF - 2);
    Monotonic<Variables> middle1 = Monotonic<Variables>::getFilledUpToIncludingLayer(MIDDLE_LAYER_OF_HIGHER_MBF - 1);
    Monotonic<Variables> middle2 = Monotonic<Variables>::getFilledUpToIncludingLayer(MIDDLE_LAYER_OF_HIGHER_MBF);

    uint64_t leftMayFormWellKnown = 0;
    uint64_t rightMayFormWellKnown = 0;
    for(size_t i = 0; i < leftSize; i++) {
        // Check that the lower part has no zeros
        // Check that the upper part has no ones
        if(andnot(middle0.bf, leftBuf[i].bf).isEmpty() && andnot(leftBuf[i].bf, middle1.bf).isEmpty()) {
            leftMayFormWellKnown++;
        }
    }
    for(size_t i = 0; i < rightSize; i++) {
        // Check that the lower part has no zeros
        // Check that the upper part has no ones
        if(andnot(middle1.bf, rightBuf[i].bf).isEmpty() && andnot(rightBuf[i].bf, middle2.bf).isEmpty()) {
            rightMayFormWellKnown++;
        }
    }

    return leftMayFormWellKnown * rightMayFormWellKnown;
}

#include "threadPool.h"
#include <string>
#include <fstream>
#include <numeric>
#include <cmath>

template<unsigned int Variables>
void estimateDPlusOneWithPairs(const std::vector<std::string>& args) {
    constexpr long double DN_SQUARED = dedekindNumbersAsDoubles[Variables] * dedekindNumbersAsDoubles[Variables];
    
    size_t blockSize = std::stoi(args[0]);

    std::ifstream file(FileName::randomMBFs(Variables), std::ios::binary);

    if (!file.is_open()) {
        std::cerr << "Error: Could not open random MBFs file" << std::endl;
        return;
    }

    std::cout << "Reading random MBFs of size " << Variables << " with Block Size " << blockSize << std::endl;

    std::mutex fileReadMutex;

    std::mutex printMutex;
    std::vector<long double> allValidFractionEstimates;
    std::vector<long double> allWellKnownMBF10Estimates;
    uint64_t totalComparisonsPassed = 0;
    uint64_t totalWellKnownMBF10 = 0;
    uint64_t checkedComparisons = 0;
    uint64_t totalSearchSpace = 0;

    size_t expectedBlockSizeInBytes = blockSize * sizeof(Monotonic<Variables>);

    runInParallelOnAllCores([&](int coreID){
        // Gets freed at program exit
        Monotonic<Variables>* thisMBFsBlock = (Monotonic<Variables>*) aligned_alloc(64, expectedBlockSizeInBytes);

        QuadraticCombinationAccumulator accum;

        // High randomness not required
        std::default_random_engine rng;

        while(true) {
            size_t numMBFsInThisBlock;
            {
                std::lock_guard<std::mutex> lg(fileReadMutex);
                
                file.read(reinterpret_cast<char*>(thisMBFsBlock), expectedBlockSizeInBytes);

                numMBFsInThisBlock = file.gcount() / sizeof(Monotonic<Variables>);
            }

            if(numMBFsInThisBlock < blockSize) {
                break;
            }

            size_t leftBufSize = numMBFsInThisBlock / 2;
            size_t rightBufSize = numMBFsInThisBlock - leftBufSize;

            {
                std::lock_guard<std::mutex> lg(printMutex);
                std::cout << "Thread " << coreID << " read " << numMBFsInThisBlock << " MBFs (" << leftBufSize << " left, " << rightBufSize << " right)" << std::endl;
            }
            Monotonic<Variables>* leftMBFs = thisMBFsBlock;
            Monotonic<Variables>* rightMBFs = thisMBFsBlock + leftBufSize;
            
            accum.reset();
            uint64_t numWellKnownMBFs = countWellKnownMBFCombinations(leftMBFs, leftBufSize, rightMBFs, rightBufSize);
            countValidCombosWithFilterTreeRecurse(leftMBFs, leftBufSize, rightMBFs, rightBufSize, rng, accum);


            long double validMatchFraction = static_cast<long double>(accum.total) / (leftBufSize * rightBufSize);
            long double wellKnownMBF10Fraction = static_cast<long double>(numWellKnownMBFs) / accum.total;

            {
                std::lock_guard<std::mutex> lg(printMutex);
                std::cout << "Thread " << coreID << " produced fraction " << validMatchFraction << " = " << accum.total << " / " << leftBufSize * rightBufSize << std::endl;
                std::cout << "Thread " << coreID << " performed " << accum.totalCalls << " total calls. " << std::endl;
                std::cout << "Thread " << coreID << " FilterTree filtered " << accum.checkedComparisons << " / " << leftBufSize * rightBufSize << " with " << static_cast<long double>(accum.checkedComparisons) / (leftBufSize * rightBufSize) << " of total search space needing to be searched. " << std::endl;
                std::cout << "Thread " << coreID << " produced well known MBF10 fraction " << wellKnownMBF10Fraction << " = " << numWellKnownMBFs << " / " << accum.total << std::endl;

                allValidFractionEstimates.push_back(validMatchFraction);
                allWellKnownMBF10Estimates.push_back(wellKnownMBF10Fraction);
                totalComparisonsPassed += accum.total;
                totalWellKnownMBF10 += numWellKnownMBFs;
                checkedComparisons += accum.checkedComparisons;
                totalSearchSpace += leftBufSize * rightBufSize;
            }
        }
    });

    std::cout << "Finished with all threads!\n\n\n" << std::endl;

    std::cout << "Found Match Fractions: " << std::endl;

    for(long double elem : allValidFractionEstimates) {
        std::cout << elem << std::endl;
    }
    std::cout << "Found Well Known MBF Fractions: " << std::endl;
    for(long double elem : allWellKnownMBF10Estimates) {
        std::cout << elem << std::endl;
    }

    std::cout << std::endl;
    std::cout << "Covered " << allValidFractionEstimates.size() << " blocks of " << blockSize << " MBFs" << std::endl;
    std::cout << "In total " << totalComparisonsPassed << " / " << checkedComparisons << " MBF pairs formed a valid MBF" << Variables+1 << std::endl;
    std::cout << "Only " << checkedComparisons << " / " << totalSearchSpace << " were actually needed. With FilterTree = " << static_cast<long double>(checkedComparisons) / totalSearchSpace << std::endl;
    std::cout << "Found " << totalWellKnownMBF10 << " well known MBF10s" << std::endl;

    long double sum = std::accumulate(allValidFractionEstimates.begin(), allValidFractionEstimates.end(), 0.0L);
    long double mean = sum / allValidFractionEstimates.size();

    long double sq_sum = std::inner_product(allValidFractionEstimates.begin(), allValidFractionEstimates.end(), allValidFractionEstimates.begin(), 0.0L);
    long double std_dev = std::sqrt(sq_sum / allValidFractionEstimates.size() - mean * mean);
    long double standardError = std_dev / std::sqrt(allValidFractionEstimates.size());
    
    std::cout << "The mean match fraction estimate = " << mean << std::endl;
    std::cout << "Std deviation = " << std_dev << std::endl;
    std::cout << "D(" << Variables + 1 << ") estimate = " << mean * DN_SQUARED << std::endl;
    std::cout << "Std deviation = " << std_dev * DN_SQUARED << std::endl;
    std::cout << "Standard Error = " << standardError * DN_SQUARED << std::endl;
    long double error_bar = 1.96 * standardError * DN_SQUARED;
    std::cout << "95% confidence interval: [" << (mean * DN_SQUARED - error_bar) << ", " << (mean * DN_SQUARED + error_bar) << "]" << std::endl;
    std::cout << std::endl;

    constexpr long double NUM_WELL_KNOWN_MBF = powl(2, choose(Variables + 1, (Variables + 1) / 2));
    long double wellKnownMBF10Mean = std::accumulate(allWellKnownMBF10Estimates.begin(), allWellKnownMBF10Estimates.end(), 0.0L) / allWellKnownMBF10Estimates.size();
    long double wellKnownMBF10SqSum = std::inner_product(allWellKnownMBF10Estimates.begin(), allWellKnownMBF10Estimates.end(), allWellKnownMBF10Estimates.begin(), 0.0L);
    long double wellKnownMBF10StdDev = std::sqrt(wellKnownMBF10SqSum / allWellKnownMBF10Estimates.size() - wellKnownMBF10Mean * wellKnownMBF10Mean);
    long double wellKnownMBF10StandardError = wellKnownMBF10StdDev / std::sqrt(allWellKnownMBF10Estimates.size());

    long double wellKnownMBF10RecipMean = 1.0L / wellKnownMBF10Mean;
    long double wellKnownMBF10RecipStdDev = wellKnownMBF10StdDev / (wellKnownMBF10Mean * wellKnownMBF10Mean);
    long double wellKnownMBF10RecipStandardError = wellKnownMBF10RecipStdDev / std::sqrt(allWellKnownMBF10Estimates.size());
    

    std::cout << "The mean well known MBF10 fraction estimate = " << wellKnownMBF10Mean << std::endl;
    std::cout << "Std deviation = " << wellKnownMBF10StdDev << std::endl;
    std::cout << "Standard Error = " << wellKnownMBF10StandardError << std::endl;
    std::cout << "D(" << Variables + 1 << ") estimate = " << wellKnownMBF10RecipMean * NUM_WELL_KNOWN_MBF << std::endl;
    std::cout << "Std deviation = " << wellKnownMBF10RecipStdDev * NUM_WELL_KNOWN_MBF << std::endl;
    std::cout << "Standard Error = " << wellKnownMBF10RecipStandardError * NUM_WELL_KNOWN_MBF << std::endl;
    long double wellKnownErrorBar = 1.96 * wellKnownMBF10RecipStandardError * NUM_WELL_KNOWN_MBF;
    std::cout << "95% confidence interval: [" << (wellKnownMBF10RecipMean * NUM_WELL_KNOWN_MBF - wellKnownErrorBar) << ", " << (wellKnownMBF10RecipMean * NUM_WELL_KNOWN_MBF + wellKnownErrorBar) << "]" << std::endl;
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

template void estimateDPlusOneWithPairs<1>(const std::vector<std::string>& args);
template void estimateDPlusOneWithPairs<2>(const std::vector<std::string>& args);
template void estimateDPlusOneWithPairs<3>(const std::vector<std::string>& args);
template void estimateDPlusOneWithPairs<4>(const std::vector<std::string>& args);
template void estimateDPlusOneWithPairs<5>(const std::vector<std::string>& args);
template void estimateDPlusOneWithPairs<6>(const std::vector<std::string>& args);
template void estimateDPlusOneWithPairs<7>(const std::vector<std::string>& args);
template void estimateDPlusOneWithPairs<8>(const std::vector<std::string>& args);
template void estimateDPlusOneWithPairs<9>(const std::vector<std::string>& args);
