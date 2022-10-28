#pragma once

#include <cstdint>

#include "synchronizedQueue.h"

#include "pcoeffClasses.h"
#include "processingContext.h"

const uint32_t* loadLinks(unsigned int Variables);

std::vector<JobTopInfo> convertTopInfos(const FlatNode* flatNodes, const std::vector<NodeIndex>& topIndices);

void runBottomBufferCreator(
	unsigned int Variables,
	PCoeffProcessingContext& context
);
