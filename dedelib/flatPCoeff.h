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

	constexpr int NODE_BLOCK_SIZE = Variables >= 7 ? 128 : 16;

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

template<unsigned int Variables>
void checkBetasCPU_MultiThread(const Monotonic<Variables>* mbfs, const OutputBuffer& buf) {
	const JobInfo& job = buf.originalInputData;

	constexpr int NODE_BLOCK_SIZE = Variables >= 7 ? 1024 : 16;

	struct ThreadData {
		Monotonic<Variables> top;
		const Monotonic<Variables>* mbfs;
		const NodeIndex* bufStart;
		const NodeIndex* bufEnd;
		const ProcessedPCoeffSum* results;
		std::atomic<const NodeIndex*> curNodeIdx;
		std::atomic<size_t> errorCount;
	};
	ThreadData data;
	data.mbfs = mbfs;
	data.top = mbfs[job.getTop()];
	data.bufStart = job.bufStart;
	data.bufEnd = job.bufEnd;
	data.results = buf.outputBuf;
	data.curNodeIdx.store(job.begin());
	data.errorCount.store(0);

	PThreadBundle allCPUs = allCoresSpread(&data, [](void* voidData) -> void* {
		ThreadData* data = (ThreadData*) voidData;
		const Monotonic<Variables>* mbfs = data->mbfs;
		Monotonic<Variables> top = data->top;
		const NodeIndex* bufStart = data->bufStart;
		const NodeIndex* bufEnd = data->bufEnd;
		const ProcessedPCoeffSum* results = data->results;
		std::atomic<const NodeIndex*>& curNodeIdx = data->curNodeIdx;

		BooleanFunction<Variables> graphsBuf[factorial(Variables)];
		const NodeIndex* nextNodeBlock = curNodeIdx.fetch_add(NODE_BLOCK_SIZE);
		while(true) {
			const NodeIndex* claimedNodeBlock = nextNodeBlock;
			nextNodeBlock = curNodeIdx.fetch_add(NODE_BLOCK_SIZE); // Fetch the next node block earlier, as not to bottleneck
			if(claimedNodeBlock >= bufEnd) break;

			const NodeIndex* claimedNodeBlockEnd = claimedNodeBlock + NODE_BLOCK_SIZE;
			if(claimedNodeBlockEnd >= bufEnd) {
				claimedNodeBlockEnd = bufEnd;
			}

			for(const NodeIndex* claimedNodeIndex = claimedNodeBlock; claimedNodeIndex != claimedNodeBlockEnd; claimedNodeIndex++) {
				Monotonic<Variables> bot = mbfs[*claimedNodeIndex];

				size_t idx = claimedNodeIndex - bufStart;
				ProcessedPCoeffSum found = results[idx];
				ProcessedPCoeffSum correct = processPCoeffSum<Variables>(top, bot, graphsBuf);

				if(found != correct) {
					std::cout << std::to_string(idx)
						 + "> sum: \033[31m" + std::to_string(getPCoeffSum(found)) + "\033[39m / \033[32m" + std::to_string(getPCoeffSum(correct))
						 + "\033[39m \tcount: \033[31m" + std::to_string(getPCoeffCount(found)) + "\033[39m / \033[32m" + std::to_string(getPCoeffCount(correct)) + "\033[39m\n" << std::flush;
				}
			}
		}

		pthread_exit(nullptr);
		return nullptr;
	});

	allCPUs.join();

	std::cout << "Number of errors: " + std::to_string(data.errorCount.load()) + "\n" << std::flush;
}

void flatDPlus1(unsigned int Variables);
void isEvenPlus2(unsigned int Variables);
u192 computeDedekindNumberFromStandardBetaTopSums(unsigned int Variables, const u128* topSums);
u192 computeDedekindNumberFromBetaSums(unsigned int Variables, const std::vector<BetaSumPair>& betaSums);
