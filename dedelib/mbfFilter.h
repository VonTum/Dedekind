
#include <stdint.h>
#include <vector>
#include <algorithm>
#include <random>
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
template<typename T, typename RNG, size_t MAX_SAMPLE_SIZE = 1024>
size_t makeSample(T* buf, size_t size, RNG& rng) {
    if(size > MAX_SAMPLE_SIZE) {
        std::uniform_int_distribution<size_t> distribution = std::uniform_int_distribution<size_t>(0, size-1);
        for(size_t i = 0; i < MAX_SAMPLE_SIZE; i++) {
            size_t selected = distribution(rng);
            T swapTmp = buf[i];
            buf[i] = buf[selected];
            buf[selected] = swapTmp;
        }
        return MAX_SAMPLE_SIZE;
    } else {
        return size;
    }
}

class MBFFilterTree {
    // How many comparisons we save it is worth to split the node for. 
    const size_t NOT_WORTH_IT_SPLIT_COUNT = 10000;

    std::vector<MBFFilterTreeNode> tree;
    uint32_t numberOfTerminalNodes = 0;
    
    size_t totalLeftMBFsInTree;
    size_t totalRightMBFsInTree;

    // Recursively split nodes and construct the tree. 
    template<unsigned int Variables>
    void constructTree(
        Monotonic<Variables>* leftBuf, size_t leftSize,
        Monotonic<Variables>* rightBuf, size_t rightSize,
        int depth, std::mt19937_64& rng
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

        uint32_t filterBit = 0; // If it's still 0, then we didn't find a suitable split so we don't split
        
        // Only try to split nodes that have some significant size
        if(rightSize >= 128 && leftSize >= 128) {
            uint64_t bestFilterOutScore = NOT_WORTH_IT_SPLIT_COUNT;
            // We don't cover 0 and 511 because they're kinda pointless. We don't have Top or Bottom in our sample
            for(uint32_t bit = 1; bit < (1 << Variables) - 1; bit++) {
                size_t leftOneCount = countWhereBitIsSet(bit, leftBuf, leftSampleSize);
                size_t rightOneCount = countWhereBitIsSet(bit, rightBuf, rightSampleSize);

                uint64_t filteredOutScore = (leftSampleSize - leftOneCount) * rightOneCount;

                if(filteredOutScore > bestFilterOutScore) {
                    filterBit = bit;
                    bestFilterOutScore = filteredOutScore;
                }
            }
        }

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
        this->constructTree(leftBuf, leftSize, rightBuf + rightZerosStartAt, rightSize - rightZerosStartAt, depth + 1, rng);
        this->tree[newNodeIndex].branchTo = this->tree.size(); // Points to the new node we're about to create

        // The ones part of right is passed, and only the left part that has ones is passed 
        this->constructTree(leftBuf, leftZerosStartAt, rightBuf, rightZerosStartAt, depth + 1, rng);
    }

public:
    template<unsigned int Variables>
    MBFFilterTree(Monotonic<Variables>* mbfBuf, size_t bufSize) {
        size_t halfBufSize = bufSize / 2;
        
        this->totalLeftMBFsInTree = halfBufSize;
        this->totalRightMBFsInTree = halfBufSize;
        
        std::mt19937_64 rng = properlySeededRNG();

        this->constructTree(mbfBuf, halfBufSize, mbfBuf + halfBufSize, halfBufSize, 0, rng);
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
