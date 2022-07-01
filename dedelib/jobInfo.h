#pragma once

#include <cstdint>
#include <cstddef>

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
