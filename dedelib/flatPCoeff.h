#pragma once

#include <atomic>

#include "knownData.h"
#include "flatMBFStructure.h"
#include "bitSet.h"
#include "u192.h"
#include "connectGraph.h"
#include "threadPool.h"

#include "pcoeffClasses.h"

#include "resultCollection.h"

#define PCOEFF_MULTITHREAD

#ifdef PCOEFF_DEDUPLICATE
constexpr size_t MAX_BUFSIZE(unsigned int Variables) {
	return mbfCounts[Variables] / 2 + 2;
}
#else
constexpr size_t MAX_BUFSIZE(unsigned int Variables) {
	return mbfCounts[Variables] + 1;
}
#endif

template<unsigned int Variables>
void flatDPlus1() {
	FlatMBFStructure<Variables> s = readFlatMBFStructure<Variables>(false, true, false, false);

	u128 totalSum = 0;
	for(size_t i = 0; i < FlatMBFStructure<Variables>::MBF_COUNT; i++) {
		ClassInfo ci = s.allClassInfos[i];
		totalSum += umul128(uint64_t(ci.intervalSizeDown), uint64_t(ci.classSize));
	}
	std::cout << "D(" << (Variables+1) << ") = " << totalSum << std::endl;
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
uint64_t computePCoeffSum(BooleanFunction<Variables> graph) {
	eliminateLeavesUp(graph);
	uint64_t connectCount = eliminateSingletons(graph); // seems to have no effect, or slight pessimization

	if(!graph.isEmpty()) {
		size_t initialGuessIndex = graph.getLast();
		BooleanFunction<Variables> initialGuess = BooleanFunction<Variables>::empty();
		initialGuess.add(initialGuessIndex);
		initialGuess = initialGuess.monotonizeDown() & graph;
		connectCount += countConnectedVeryFast(graph, initialGuess);
	}
	return uint64_t(1) << connectCount;
}

template<unsigned int Variables>
uint64_t computePCoeffSum(const BooleanFunction<Variables>* graphsBuf, const BooleanFunction<Variables>* graphsBufEnd) {
	uint64_t totalSum = 0;

	for(; graphsBuf != graphsBufEnd; graphsBuf++) {
		totalSum += computePCoeffSum<Variables>(*graphsBuf);
	}
	return totalSum;
}

template<typename TI>
TI getBitField(TI value, int startAt, int bitWidth) {
	return (value >> startAt) & ((static_cast<TI>(1) << bitWidth) - static_cast<TI>(1));
}

template<unsigned int Variables>
ProcessedPCoeffSum processPCoeffSum(Monotonic<Variables> top, Monotonic<Variables> bot) {
	uint64_t pcoeffSum = 0;
	uint64_t pcoeffCount = 0;
	bot.forEachPermutation([&](const Monotonic<Variables>& permutedBot) {
		if(permutedBot <= top) {
			BooleanFunction<Variables> graph = andnot(top.bf, permutedBot.bf);
			pcoeffCount++;
			pcoeffSum += computePCoeffSum<Variables>(graph);
		}
	});

	return produceProcessedPcoeffSumCount(pcoeffSum, pcoeffCount);
}

template<unsigned int Variables>
ProcessedPCoeffSum processPCoeffSum(Monotonic<Variables> top, Monotonic<Variables> bot, BooleanFunction<Variables> graphsBuf[factorial(Variables)]) {
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

	for(const NodeIndex* cur = job.begin(); cur != job.end(); cur++) {
		Monotonic<Variables> bot = mbfs[*cur];
		countConnectedSumBuf[job.indexOf(cur)] = processPCoeffSum<Variables>(top, bot, graphsBuf);
	}
}

template<unsigned int Variables>
void processBetasCPU_MultiThread(const Monotonic<Variables>* mbfs, const JobInfo& job, ProcessedPCoeffSum* countConnectedSumBuf, ThreadPool& threadPool) {
	Monotonic<Variables> top = mbfs[job.getTop()];

	constexpr int NODE_BLOCK_SIZE = Variables >= 7 ? 128 : 16;

	std::atomic<const NodeIndex*> i;
	i.store(job.begin());

	threadPool.doInParallel([&](){
		BooleanFunction<Variables> graphsBuf[factorial(Variables)];
		while(true) {
			const NodeIndex* claimedNodeBlock = i.fetch_add(NODE_BLOCK_SIZE);
			if(claimedNodeBlock >= job.end()) break;

			const NodeIndex* claimedNodeBlockEnd = claimedNodeBlock + NODE_BLOCK_SIZE;
			if(claimedNodeBlockEnd >= job.end()) {
				claimedNodeBlockEnd = job.end();
			}

			for(const NodeIndex* claimedNodeIndex = claimedNodeBlock; claimedNodeIndex != claimedNodeBlockEnd; claimedNodeIndex++) {
				Monotonic<Variables> bot = mbfs[*claimedNodeIndex];

				countConnectedSumBuf[job.indexOf(claimedNodeIndex)] = processPCoeffSum<Variables>(top, bot, graphsBuf);
			}
		}
	});
}

template<unsigned int Variables>
void isEvenPlus2() {
	FlatMBFStructure<Variables> allMBFs = readFlatMBFStructure<Variables>(false, true, true, false);

	bool isEven = true; // 0 is even
	for(NodeIndex i = 0; i < mbfCounts[Variables]; i++) {
		uint64_t classSize = allMBFs.allClassInfos[i].classSize;

		if(classSize % 2 == 0) continue;

		uint64_t intervalSizeDown = allMBFs.allClassInfos[i].intervalSizeDown;
		if(intervalSizeDown % 2 == 0) continue;

		NodeIndex dualI = allMBFs.allNodes[i].dual;
		uint64_t intervalSizeUp = allMBFs.allClassInfos[dualI].intervalSizeDown;
		if(intervalSizeUp % 2 == 0) continue;

		isEven = !isEven;
	}

	std::cout << "D(" << (Variables + 2) << ") is " << (isEven ? "even" : "odd") << std::endl;
}

template<unsigned int Variables>
u192 computeDedekindNumberFromBetaSums(const FlatMBFStructure<Variables>& allMBFData, const std::vector<BetaSum>& betaSums) {
	if(betaSums.size() != mbfCounts[Variables]) {
		std::cerr << "Incorrect number of beta sums! Aborting!" << std::endl;
		std::abort();
	}
	u192 total = 0;
	for(size_t i = 0; i < betaSums.size(); i++) {
		BetaSum betaSum = betaSums[i];
		ClassInfo topInfo = allMBFData.allClassInfos[i];
		
		// invalid for DEDUPLICATION
#ifndef PCOEFF_DEDUPLICATE
		if(betaSum.countedIntervalSizeDown != topInfo.intervalSizeDown) throw "INVALID!";
#endif
		uint64_t topIntervalSizeUp = allMBFData.allClassInfos[allMBFData.allNodes[i].dual].intervalSizeDown;
		uint64_t topFactor = topIntervalSizeUp * topInfo.classSize; // max log2(2414682040998*5040) = 53.4341783883
		total += umul192(betaSum.betaSum, topFactor);
	}
	return total;
}

