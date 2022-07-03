#pragma once

#include "u192.h"
#include "pcoeffClasses.h"
#include "flatMBFStructure.h"

#include "synchronizedQueue.h"

BetaSum produceBetaTerm(ClassInfo info, uint64_t pcoeffSum, uint64_t pcoeffCount);

BetaSum sumOverBetas(const ClassInfo* mbfClassInfos, const NodeIndex* idxBuf, const NodeIndex* bufEnd, const ProcessedPCoeffSum* countConnectedSumBuf);

template<unsigned int Variables>
BetaSum produceBetaResult(const FlatMBFStructure<Variables>& allMBFData, const JobInfo& curJob, const ProcessedPCoeffSum* pcoeffSumBuf) {
	// Skip the first element, as it is the top
	BetaSum jobSum = sumOverBetas(allMBFData.allClassInfos, curJob.begin(), curJob.end(), pcoeffSumBuf + JobInfo::FIRST_BOT_OFFSET);

#ifdef PCOEFF_DEDUPLICATE
	ProcessedPCoeffSum nonDuplicateTopDual = processOneBeta(allMBFData, curJob.getTop(), curJob.topDual);

	ClassInfo info = allMBFData.allClassInfos[curJob.topDual];

	BetaSum nonDuplicateTopDualResult = produceBetaTerm(info, getPCoeffSum(nonDuplicateTopDual), getPCoeffCount(nonDuplicateTopDual));

	jobSum = jobSum + jobSum + nonDuplicateTopDualResult;
#endif
	return jobSum / factorial(Variables);
}

template<unsigned int Variables>
void resultProcessor(
	const FlatMBFStructure<Variables>& allMBFData,
	SynchronizedQueue<OutputBuffer>& outputQueue,
	SynchronizedStack<NodeIndex*>& inputBufferReturnQueue,
	SynchronizedStack<ProcessedPCoeffSum*>& outputBufferReturnQueue,
	std::vector<BetaResult>& finalResults
) {
	std::cout << "Result processor started.\n" << std::flush;
	for(std::optional<OutputBuffer> outputBuffer; (outputBuffer = outputQueue.pop_wait()).has_value(); ) {
		OutputBuffer outBuf = outputBuffer.value();

		BetaResult curBetaResult;
		//if constexpr(Variables == 7) std::cout << "Results for job " << outBuf.originalInputData.getTop() << std::endl;
		curBetaResult.topIndex = outBuf.originalInputData.getTop();
		curBetaResult.betaSum = produceBetaResult(allMBFData, outBuf.originalInputData, outBuf.outputBuf);

		inputBufferReturnQueue.push(outBuf.originalInputData.bufStart);
		outputBufferReturnQueue.push(outBuf.outputBuf);

		finalResults.push_back(curBetaResult);
		if constexpr(Variables == 7) std::cout << '.' << std::flush;
		if(finalResults.size() % 1000 == 0) std::cout << std::endl << finalResults.size() << std::endl;
	}
	std::cout << "Result processor finished.\n" << std::flush;
}
