#include "mbfFilter.h"

#include <iostream>
#include <iomanip>

#include "flatBufferManagement.h"
#include "fileNames.h"
#include "toString.h"

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
	size_t numRandomMBF9 = 2*1024*1024;
	Monotonic<9>* randomMBF9Buf = const_cast<Monotonic<9>*>(readFlatBuffer<Monotonic<9>>(FileName::randomMBFs(9), numRandomMBF9));

	static_assert(sizeof(Monotonic<9>) == 64);

	std::cout << "Loaded " << numRandomMBF9 << " MBF9" << std::endl;

    std::cout << "Last Mbf: " << randomMBF9Buf[numRandomMBF9 - 1] << std::endl;

    for(size_t i = 0; i < numRandomMBF9 - 1; i++) {
        std::swap(randomMBF9Buf[i], randomMBF9Buf[i+1]);
    }

    MBFFilterTree tree(randomMBF9Buf, numRandomMBF9);

    tree.debugPrintTree();
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
