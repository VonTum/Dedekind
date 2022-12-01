#pragma once

#include <atomic>

#include "knownData.h"
#include "bitSet.h"
#include "u192.h"
#include "connectGraph.h"
#include "threadPool.h"

#include "pcoeffClasses.h"

#include "resultCollection.h"

#define PCOEFF_MULTITHREAD

template<unsigned int Variables>
uint64_t computePCoeffSum(const BooleanFunction<Variables>* graphsBuf, const BooleanFunction<Variables>* graphsBufEnd) {
	uint64_t totalSum = 0;

	for(; graphsBuf != graphsBufEnd; graphsBuf++) {
		totalSum += uint64_t(1) << countConnectedVeryFast<Variables>(*graphsBuf);
	}
	return totalSum;
}

template<unsigned int Variables>
ProcessedPCoeffSum processPCoeffSum(Monotonic<Variables> top, Monotonic<Variables> bot) {
	uint64_t pcoeffSum = 0;
	uint64_t pcoeffCount = 0;
	bot.forEachPermutation([&](const Monotonic<Variables>& permutedBot) {
		if(permutedBot <= top) {
			BooleanFunction<Variables> graph = andnot(top.bf, permutedBot.bf);
			pcoeffCount++;
			pcoeffSum += uint64_t(1) << countConnectedVeryFast<Variables>(graph);
		}
	});

	return produceProcessedPcoeffSumCount(pcoeffSum, pcoeffCount);
}

template<unsigned int Variables>
BooleanFunction<Variables>* listPermutationsBelow(const Monotonic<Variables>& top, const Monotonic<Variables>& botToPermute, BooleanFunction<Variables> result[factorial(Variables)]) {
	botToPermute.forEachPermutation([&result, &top](const Monotonic<Variables>& permutedBot) {
		if(permutedBot <= top) {
			BooleanFunction<Variables> difference = andnot(top.bf, permutedBot.bf);
			*result++ = difference;
		}
	});
	return result;
}

template<unsigned int Variables>
ProcessedPCoeffSum processPCoeffSum(Monotonic<Variables> top, Monotonic<Variables> bot, BooleanFunction<Variables>* graphsBuf/*[factorial(Variables)]*/) {
	BooleanFunction<Variables>* graphsBufEnd = listPermutationsBelow<Variables>(top, bot, graphsBuf);
	uint64_t pcoeffCount = graphsBufEnd - graphsBuf;
	uint64_t pcoeffSum = computePCoeffSum(graphsBuf, graphsBufEnd);
	return produceProcessedPcoeffSumCount(pcoeffSum, pcoeffCount);
}




template<unsigned int Variables>
ProcessedPCoeffSum processOneBeta(const Monotonic<Variables>* mbfs, NodeIndex topIdx, NodeIndex botIdx) {
	Monotonic<Variables> top = mbfs[topIdx];
	Monotonic<Variables> bot = mbfs[botIdx];

	//BooleanFunction<Variables> graphsBuf[factorial(Variables)];
	return processPCoeffSum<Variables>(top, bot/*, graphsBuf*/);
}

template<unsigned int Variables>
void processBetasCPU_SingleThread(const Monotonic<Variables>* mbfs, const JobInfo& job, ProcessedPCoeffSum* countConnectedSumBuf) {
	Monotonic<Variables> top = mbfs[job.getTop()];

	BooleanFunction<Variables> graphsBuf[factorial(Variables)];

	const NodeIndex* jobEnd = job.end();

	for(const NodeIndex* cur = job.begin(); cur != jobEnd; cur++) {
		Monotonic<Variables> bot = mbfs[*cur];
		countConnectedSumBuf[job.indexOf(cur)] = processPCoeffSum<Variables>(top, bot, graphsBuf);
	}
}

template<unsigned int Variables>
void processBetasCPU_MultiThread(const Monotonic<Variables>* mbfs, const JobInfo& job, ProcessedPCoeffSum* countConnectedSumBuf, ThreadPool& threadPool) {
	Monotonic<Variables> top = mbfs[job.getTop()];

	constexpr int NODE_BLOCK_SIZE = Variables >= 7 ? 1024 : 16;

	std::atomic<const NodeIndex*> i;
	i.store(job.begin());

	const NodeIndex* jobEnd = job.end();

	threadPool.doInParallel([&](){
		BooleanFunction<Variables> graphsBuf[factorial(Variables)];
		const NodeIndex* nextNodeBlock = i.fetch_add(NODE_BLOCK_SIZE);
		while(true) {
			const NodeIndex* claimedNodeBlock = nextNodeBlock;
			nextNodeBlock = i.fetch_add(NODE_BLOCK_SIZE); // Fetch the next node block earlier, as not to bottleneck
			if(claimedNodeBlock >= jobEnd) break;

			const NodeIndex* claimedNodeBlockEnd = claimedNodeBlock + NODE_BLOCK_SIZE;
			if(claimedNodeBlockEnd >= jobEnd) {
				claimedNodeBlockEnd = jobEnd;
			}

			for(const NodeIndex* claimedNodeIndex = claimedNodeBlock; claimedNodeIndex != claimedNodeBlockEnd; claimedNodeIndex++) {
				Monotonic<Variables> bot = mbfs[*claimedNodeIndex];

				countConnectedSumBuf[job.indexOf(claimedNodeIndex)] = processPCoeffSum<Variables>(top, bot, graphsBuf);
			}
		}
	});
}

void flatDPlus1(unsigned int Variables);
void isEvenPlus2(unsigned int Variables);
u192 computeDedekindNumberFromStandardBetaTopSums(unsigned int Variables, const u128* topSums);
u192 computeDedekindNumberFromBetaSums(unsigned int Variables, const std::vector<BetaSumPair>& betaSums);
