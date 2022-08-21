#pragma once

#include <vector>

#include "u192.h"
#include "pcoeffClasses.h"

#include "synchronizedQueue.h"

#include "processingContext.h"

#include "threadPool.h"

BetaSum produceBetaTerm(ClassInfo info, uint64_t pcoeffSum, uint64_t pcoeffCount);

BetaSum sumOverBetas(const ClassInfo* mbfClassInfos, const NodeIndex* idxBuf, const NodeIndex* bufEnd, const ProcessedPCoeffSum* countConnectedSumBuf);

BetaSum produceBetaResult(unsigned int Variables, const ClassInfo* mbfClassInfos, const JobInfo& curJob, const ProcessedPCoeffSum* pcoeffSumBuf);

std::vector<BetaResult> resultProcessor(
	unsigned int Variables,
	PCoeffProcessingContext& context,
	size_t numResults
);

// Expects mbfs0 to be stored in memory of socket0, and mbfs1 to have memory of socket1
std::vector<BetaResult> NUMAResultProcessorWithValidator(
	unsigned int Variables,
	PCoeffProcessingContext& context,
	size_t numResults,
	size_t numValidators,
	void(*validator)(const OutputBuffer&, const void*, ThreadPool&),
	const void* mbfs[2]
);
