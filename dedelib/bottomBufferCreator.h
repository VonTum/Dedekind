#pragma once

#include <cstdint>

#include "synchronizedQueue.h"

#include "jobInfo.h"

#define PCOEFF_DEDUPLICATE

struct JobTopInfo {
	uint32_t top;
	uint32_t topDual;
};

const uint32_t* loadLinks(unsigned int Variables);

void runBottomBufferCreatorNoAlloc (
	unsigned int Variables,
	const std::vector<JobTopInfo>& jobTops,
	SynchronizedQueue<JobInfo>& outputQueue,
	SynchronizedStack<uint32_t*>& returnQueue,
	const uint32_t* links,
	uint64_t* swapperA,
	uint64_t* swapperB
);

void runBottomBufferCreator(
	unsigned int Variables,
	const std::vector<JobTopInfo>& jobTops,
	SynchronizedQueue<JobInfo>& outputQueue,
	SynchronizedStack<uint32_t*>& returnQueue
);
