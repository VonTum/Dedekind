#pragma once

#include <cstdint>
#include <future>

#include "synchronizedQueue.h"

#include "pcoeffClasses.h"

struct JobTopInfo {
	uint32_t top;
	uint32_t topDual;
};

const uint32_t* loadLinks(unsigned int Variables);

void runBottomBufferCreator(
	unsigned int Variables,
	std::future<std::vector<JobTopInfo>>& jobTops,
	SynchronizedQueue<JobInfo>& outputQueue,
	SynchronizedStack<uint32_t*>& returnQueue,
	int numberOfThreads = 1
);
