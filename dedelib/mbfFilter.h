
#include <stdint.h>
#include <vector>
#include <algorithm>
#include <random>
#include <string.h>
#include "funcTypes.h"
#include "generators.h"

#define MAKE_STATISTICS



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
size_t partitionByBit(uint32_t bit, Monotonic<Variables>* buf, size_t bufSize) {
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
size_t makeSample(T* buf, size_t size, RNG& rng) {
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
static void fastCountPerBit(const IntT *__restrict asBits, size_t numBitsets, IntT *__restrict counts) {
    constexpr size_t BitsPerPart = sizeof(IntT) * 8;
    constexpr size_t PartsPerMBF = TotalBits / BitsPerPart;

    // Make sure our count can still contain 
    assert(numBitsets < (1 << BitsPerPart));
    
    memset(counts, 0, TotalBits * sizeof(IntT));

    // Only vectorizes well with clang
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
static void checkFastCountPerBitBuffer(const Monotonic<Variables>* mbfs, size_t numMBFs, const IntT* counts) {
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

    for(size_t i = 0; i < 1 << Variables; i++) {
        //std::cout << i << ": " << int(counts[i]) << " " << int(newArray[i]) << std::endl;
        assert(counts[i] == newArray[i]);
    }
}

template<typename IntT, unsigned int Variables>
size_t findBestBitToSplitOver(
    const Monotonic<Variables>* leftMBFs, const Monotonic<Variables>* rightMBFs,
    size_t leftSampleSize, size_t rightSampleSize,
    size_t leftSize, size_t rightSize
) {
    constexpr size_t TotalBits = 1 << Variables;
    constexpr size_t BitsPerPart = sizeof(IntT) * 8;
    constexpr size_t PartsPerMBF = TotalBits / BitsPerPart;
    // How many comparisons we save it is worth to split the node for. 
    constexpr uint64_t NOT_WORTH_IT_SPLIT_COUNT = 1000;

    // This factor is to calculate the expected total amount of checks that could be avoided
    double sampleToTotalSizeFactor = static_cast<double>(leftSampleSize) * rightSampleSize / (leftSize * rightSize);
    
    uint64_t minimumSavingsForSplit = NOT_WORTH_IT_SPLIT_COUNT * sampleToTotalSizeFactor;

    const IntT* leftAsBits = reinterpret_cast<const IntT*>(leftMBFs);
    const IntT* rightAsBits = reinterpret_cast<const IntT*>(rightMBFs);

    alignas(64) IntT leftCounts[TotalBits];
    alignas(64) IntT rightCounts[TotalBits];

    fastCountPerBit<IntT, TotalBits>(leftAsBits, leftSampleSize, leftCounts);
    fastCountPerBit<IntT, TotalBits>(rightAsBits, rightSampleSize, rightCounts);

    //checkFastCountPerBitBuffer(leftMBFs, numLeftMBFs, leftCounts);
    //checkFastCountPerBitBuffer(rightMBFs, numRightMBFs, rightCounts);

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


template<unsigned int Variables>
void testFilterTreePerformance();

extern template void testFilterTreePerformance<1>();
extern template void testFilterTreePerformance<2>();
extern template void testFilterTreePerformance<3>();
extern template void testFilterTreePerformance<4>();
extern template void testFilterTreePerformance<5>();
extern template void testFilterTreePerformance<6>();
extern template void testFilterTreePerformance<7>();
extern template void testFilterTreePerformance<8>();
extern template void testFilterTreePerformance<9>();

template<unsigned int Variables>
void testTreeLessFilterTreePerformance();
extern template void testTreeLessFilterTreePerformance<1>();
extern template void testTreeLessFilterTreePerformance<2>();
extern template void testTreeLessFilterTreePerformance<3>();
extern template void testTreeLessFilterTreePerformance<4>();
extern template void testTreeLessFilterTreePerformance<5>();
extern template void testTreeLessFilterTreePerformance<6>();
extern template void testTreeLessFilterTreePerformance<7>();
extern template void testTreeLessFilterTreePerformance<8>();
extern template void testTreeLessFilterTreePerformance<9>();
