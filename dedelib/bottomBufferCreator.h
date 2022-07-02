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

void runBottomBufferCreator(
	unsigned int Variables,
	const std::vector<JobTopInfo>& jobTops,
	SynchronizedQueue<JobInfo>& outputQueue,
	SynchronizedStack<uint32_t*>& returnQueue,
	int numberOfThreads = 1
);
