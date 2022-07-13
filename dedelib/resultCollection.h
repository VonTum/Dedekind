#pragma once

#include "u192.h"
#include "pcoeffClasses.h"

#include "synchronizedQueue.h"

BetaSum produceBetaTerm(ClassInfo info, uint64_t pcoeffSum, uint64_t pcoeffCount);

BetaSum sumOverBetas(const ClassInfo* mbfClassInfos, const NodeIndex* idxBuf, const NodeIndex* bufEnd, const ProcessedPCoeffSum* countConnectedSumBuf);

BetaSum produceBetaResult(unsigned int Variables, const ClassInfo* mbfClassInfos, const JobInfo& curJob, const ProcessedPCoeffSum* pcoeffSumBuf);

void resultProcessor(
	unsigned int Variables,
	SynchronizedQueue<OutputBuffer>& outputQueue,
	SynchronizedStack<NodeIndex*>& inputBufferReturnQueue,
	SynchronizedSlabAllocator<ProcessedPCoeffSum>& outputBufferReturnQueue,
	std::vector<BetaResult>& finalResults
);
