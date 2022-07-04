#pragma once

#include "u192.h"
#include <cstdint>
#include <cstddef>
#include <cassert>

#define PCOEFF_DEDUPLICATE

#ifdef PCOEFF_DEDUPLICATE
	constexpr int BUF_BOTTOM_OFFSET = 3; // Two copies of Top and the dual of the top. This dual may not be a valid bottom, so it may have result 0
	constexpr int TOP_DUAL_INDEX = 2;
#else
	constexpr int BUF_BOTTOM_OFFSET = 2; // Two copies of Top
#endif

typedef uint32_t NodeIndex;

struct JobInfo {
	NodeIndex topDual;
	int topLayer;
	NodeIndex* bufStart;
	NodeIndex* bufEnd;

	static constexpr const size_t FIRST_BOT_OFFSET = 2;

	void initialize(NodeIndex top, NodeIndex topDual, int topLayer) {
		this->topDual = topDual;
		this->topLayer = topLayer;
		this->bufEnd = this->bufStart;

		// Add top twice, because FPGA compute expects pairs. 
		for(size_t i = 0; i < FIRST_BOT_OFFSET; i++) {
			*bufEnd++ = top | 0x80000000;
		}
	} 

	void add(NodeIndex newBot) {
		*bufEnd++ = newBot;
	}

	NodeIndex getTop() const {
		return (*bufStart) & 0x7FFFFFFF;
	}

	size_t size() const {
		return bufEnd - bufStart;
	}

	// Iterates over only the bottoms
	NodeIndex* begin() const {return bufStart + FIRST_BOT_OFFSET;}
	NodeIndex* end() const {return bufEnd;}

	size_t getNumberOfBottoms() const {
		return end() - begin();
	}

	size_t indexOf(const NodeIndex* ptr) const {
		return ptr - bufStart;
	}
};

typedef uint32_t NodeOffset;
typedef uint64_t LinkIndex;

struct ClassInfo {
	uint64_t intervalSizeDown : 48; // log2(2414682040998) == 41.1349703699
	uint64_t classSize : 16; // log2(5040) == 12.2992080184
};

struct FlatNode {
	// dual could be NodeOffset instead of NodeIndex, Requiring only log2(16440466) == 23.9707478566
	/*NodeIndex*/ uint64_t dual : 30; // log2(490013148) == 28.8682452191
	/*LinkIndex*/ uint64_t downLinks : 34; // log2(7329014832) == 32.770972138
	// this second downLinks index marks a list going from this->downLinks to (this+1)->downLinks
};

typedef uint64_t ProcessedPCoeffSum;

inline ProcessedPCoeffSum produceProcessedPcoeffSumCount(uint64_t pcoeffSum, uint64_t pcoeffCount) {
	assert(pcoeffSum < (uint64_t(1) << 48));
	assert(pcoeffCount < (uint64_t(1) << 13));

	return (pcoeffCount << 48) | pcoeffSum;
}

inline uint64_t getPCoeffSum(ProcessedPCoeffSum input) {
	return input & 0x0000FFFFFFFFFFFF;
}

inline uint64_t getPCoeffCount(ProcessedPCoeffSum input) {
	return (input & 0x1FFF000000000000) >> 48;
}


struct BetaSum {
	u128 betaSum;
	
	// validation data
	uint64_t countedIntervalSizeDown;
};

inline BetaSum operator+(BetaSum a, BetaSum b) {
	return BetaSum{a.betaSum + b.betaSum, a.countedIntervalSizeDown + b.countedIntervalSizeDown};
}
inline BetaSum& operator+=(BetaSum& a, BetaSum b) {
	a.betaSum += b.betaSum;
	a.countedIntervalSizeDown += b.countedIntervalSizeDown;
	return a;
}
inline BetaSum operator/(BetaSum a, uint32_t b) {
	return BetaSum{a.betaSum / b, a.countedIntervalSizeDown / b};
}

struct OutputBuffer {
	JobInfo originalInputData;
	ProcessedPCoeffSum* outputBuf;
};


struct BetaResult {
	BetaSum betaSum;
	NodeIndex topIndex;
};

class BetaResultCollector {
	std::vector<BetaSum> allBetaSums;
	std::vector<bool> hasSeenResult;

public:
	BetaResultCollector(unsigned int Variables);
	void addBetaResult(BetaResult result);
	void addBetaResults(const std::vector<BetaResult>& results);
	std::vector<BetaSum> getResultingSums();
};


