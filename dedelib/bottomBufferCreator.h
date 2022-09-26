#pragma once

#include <cstdint>
#include <future>

#include "synchronizedQueue.h"

#include "pcoeffClasses.h"
#include "processingContext.h"

class Latch;

struct JobTopInfo {
	uint32_t top;
	uint32_t topDual;
};

const uint32_t* loadLinks(unsigned int Variables);

std::vector<JobTopInfo> convertTopInfos(const FlatNode* flatNodes, const std::vector<NodeIndex>& topIndices);

void runBottomBufferCreator(
	unsigned int Variables,
	std::future<std::vector<JobTopInfo>>& jobTops,
	PCoeffProcessingContext& context,
	int numberOfThreads = 1,
	Latch* hasStarted = nullptr
);

void runBottomBufferCreator(
	unsigned int Variables,
	const std::vector<JobTopInfo>& jobTops,
	PCoeffProcessingContext& context,
	int numberOfThreads = 1,
	Latch* hasStarted = nullptr
);

void runBottomBufferCreator(
	unsigned int Variables,
	const std::vector<NodeIndex>& jobTops,
	PCoeffProcessingContext& context,
	int numberOfThreads = 1,
	Latch* hasStarted = nullptr
);
