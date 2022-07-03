#pragma once

#include <atomic>

#include "knownData.h"
#include "flatMBFStructure.h"
#include "swapperLayers.h"
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

template<unsigned int Variables, size_t BatchSize>
void followNextLayerLinks(const FlatMBFStructure<Variables>& downLinkStructure, SwapperLayers<Variables, BitSet<BatchSize>>& swapper) {
	int curLayer = swapper.getCurrentLayerDownward();
	const FlatNode* firstNode = downLinkStructure.allNodes + flatNodeLayerOffsets[Variables][curLayer];
	for(size_t i = 0; i < swapper.getSourceLayerSize(); i++) {
		BitSet<BatchSize> sourceElem = swapper.source(i);
		if(sourceElem.isEmpty()) continue; // skip nonreached nodes

		const NodeOffset* sourceNodeDownLinksStart = downLinkStructure.allLinks + firstNode[i].downLinks;
		const NodeOffset* sourceNodeDownLinksEnd = downLinkStructure.allLinks + firstNode[i+1].downLinks;

		for(const NodeOffset* curDownlink = sourceNodeDownLinksStart; curDownlink != sourceNodeDownLinksEnd; curDownlink++) {
			swapper.dest(*curDownlink) |= sourceElem;
		}
	}

	swapper.pushNext();
}

template<unsigned int Variables, size_t BatchSize>
struct JobBatch {
	JobInfo jobs[BatchSize];
	size_t jobCount;

	void initialize(const FlatMBFStructure<Variables>& downLinkStructure, NodeIndex* tops, size_t jobCount = BatchSize) {
		assert(jobCount <= BatchSize);
		for(size_t i = 0; i < jobCount; i++) {
			JobInfo& job = jobs[i];
			NodeIndex top = tops[i];
			NodeIndex topDual = downLinkStructure.allNodes[top].dual;
			int topLayer = getFlatLayerOfIndex(Variables, top);
			job.initialize(top, topDual, topLayer);
		}
		this->jobCount = jobCount;
	}

	int getHighestLayer() const {
		int highestLayer = jobs[0].topLayer;
		for(size_t i = 1; i < jobCount; i++){
			const JobInfo& j = jobs[i];
			if(j.topLayer > highestLayer) highestLayer = j.topLayer;
		}
		return highestLayer;
	}
	
	int getLowestLayer() const {
		int lowestLayer = jobs[0].topLayer;
		for(size_t i = 1; i < jobCount; i++){
			const JobInfo& j = jobs[i];
			if(j.topLayer < lowestLayer) lowestLayer = j.topLayer;
		}
		return lowestLayer;
	}
};

template<unsigned int Variables, size_t BatchSize>
void addSourceElementsToJobs(const SwapperLayers<Variables, BitSet<BatchSize>>& swapper, JobBatch<Variables, BatchSize>& jobBatch) {
	NodeIndex firstNodeInLayer = FlatMBFStructure<Variables>::cachedOffsets[swapper.getCurrentLayerDownward()];
	for(NodeOffset i = 0; i < NodeOffset(swapper.getSourceLayerSize()); i++) {
		BitSet<BatchSize> includedBits = swapper.source(i);
		if(includedBits.isEmpty()) continue;

		NodeIndex curNodeIndex = firstNodeInLayer + i;

		for(size_t jobIdx = 0; jobIdx < BatchSize; jobIdx++) {
			if(includedBits.get(jobIdx)) {
				JobInfo& job = jobBatch.jobs[jobIdx];
				job.add(curNodeIndex);
			}
		}
	}
}

template<unsigned int Variables, size_t BatchSize>
void addSourceElementsToJobsWithDeDuplication(const SwapperLayers<Variables, BitSet<BatchSize>>& swapper, JobBatch<Variables, BatchSize>& jobBatch) {
	NodeIndex firstNodeInLayer = flatNodeLayerOffsets[Variables][swapper.getCurrentLayerDownward()];
	for(NodeOffset i = 0; i < NodeOffset(swapper.getSourceLayerSize()); i++) {
		BitSet<BatchSize> includedBits = swapper.source(i);
		if(includedBits.isEmpty()) continue;

		NodeIndex curNodeIndex = firstNodeInLayer + i;

		for(size_t jobIdx = 0; jobIdx < BatchSize; jobIdx++) {
			if(includedBits.get(jobIdx)) {
				JobInfo& job = jobBatch.jobs[jobIdx];
				if(job.topDual > curNodeIndex) {// Arbitrary Equivalence Class Ordering for Deduplication
					job.add(curNodeIndex);
				}
			}
		}
	}
}


template<unsigned int Variables, size_t BatchSize>
void initializeSwapper(SwapperLayers<Variables, BitSet<BatchSize>>& swapper, const JobBatch<Variables, BatchSize>& jobBatch) {
	int curSwapperLayer = swapper.getCurrentLayerDownward();
	NodeIndex layerStart = flatNodeLayerOffsets[Variables][curSwapperLayer];
	for(size_t i = 0; i < jobBatch.jobCount; i++) {
		if(jobBatch.jobs[i].topLayer == curSwapperLayer) {
			NodeOffset indexInLayer = jobBatch.jobs[i].getTop() - layerStart;
			swapper.source(indexInLayer).set(i);
		}
	}
}

template<unsigned int Variables, size_t BatchSize>
void computeBuffers(const FlatMBFStructure<Variables>& downLinkStructure, JobBatch<Variables, BatchSize>& jobBatch, SwapperLayers<Variables, BitSet<BatchSize>>& swapper) {
	int currentLayer = jobBatch.getHighestLayer();

	swapper.resetDownward(currentLayer);

	for(; ; currentLayer--) {
		initializeSwapper(swapper, jobBatch);

		addSourceElementsToJobs(swapper, jobBatch);

		if(currentLayer <= 0) break;
		followNextLayerLinks(downLinkStructure, swapper);
	}

	assert(!swapper.canPushNext());
}

template<unsigned int Variables, size_t BatchSize>
void computeBuffersDeduplicate(const FlatMBFStructure<Variables>& downLinkStructure, JobBatch<Variables, BatchSize>& jobBatch, SwapperLayers<Variables, BitSet<BatchSize>>& swapper) {
	int currentLayer = jobBatch.getHighestLayer();

	int highestDualLayer = (1 << Variables) - jobBatch.getLowestLayer();

	swapper.resetDownward(currentLayer);

	for(; ; currentLayer--) {
		initializeSwapper(swapper, jobBatch);

		// this if statement optimizes out a lot of unneccecary memory reading for job elements which won't be 
		// touched anyways because of deduplication
		if(currentLayer <= highestDualLayer) { 
			addSourceElementsToJobsWithDeDuplication(swapper, jobBatch);
		}

		if(currentLayer <= 0) break;
		followNextLayerLinks(downLinkStructure, swapper);
	}

	assert(!swapper.canPushNext());
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
uint64_t computePCoeffSum(const BooleanFunction<Variables>* graphsBuf, const BooleanFunction<Variables>* graphsBufEnd) {
	uint64_t totalSum = 0;

	for(; graphsBuf != graphsBufEnd; graphsBuf++) {
		BooleanFunction<Variables> graph = *graphsBuf;

		eliminateLeavesUp(graph);
		uint64_t connectCount = eliminateSingletons(graph); // seems to have no effect, or slight pessimization

		if(!graph.isEmpty()) {
			size_t initialGuessIndex = graph.getLast();
			BooleanFunction<Variables> initialGuess = BooleanFunction<Variables>::empty();
			initialGuess.add(initialGuessIndex);
			initialGuess = initialGuess.monotonizeDown() & graph;
			connectCount += countConnectedVeryFast(graph, initialGuess);
		}
		totalSum += uint64_t(1) << connectCount;
	}
	return totalSum;
}

template<typename TI>
TI getBitField(TI value, int startAt, int bitWidth) {
	return (value >> startAt) & ((static_cast<TI>(1) << bitWidth) - static_cast<TI>(1));
}

template<unsigned int Variables>
ProcessedPCoeffSum processPCoeffSum(Monotonic<Variables> top, Monotonic<Variables> bot, BooleanFunction<Variables> graphsBuf[factorial(Variables)]) {
	BooleanFunction<Variables>* graphsBufEnd = listPermutationsBelow<Variables>(top, bot, graphsBuf);
	uint64_t pcoeffCount = graphsBufEnd - graphsBuf;
	uint64_t pcoeffSum = computePCoeffSum(graphsBuf, graphsBufEnd);
	return produceProcessedPcoeffSumCount(pcoeffSum, pcoeffCount);
}

template<unsigned int Variables>
ProcessedPCoeffSum processOneBeta(const FlatMBFStructure<Variables>& downLinkStructure, NodeIndex topIdx, NodeIndex botIdx) {
	Monotonic<Variables> top = downLinkStructure.mbfs[topIdx];
	Monotonic<Variables> bot = downLinkStructure.mbfs[botIdx];

	BooleanFunction<Variables> graphsBuf[factorial(Variables)];
	return processPCoeffSum<Variables>(top, bot, graphsBuf);
}

template<unsigned int Variables>
void processBetasCPU_SingleThread(const FlatMBFStructure<Variables>& downLinkStructure, const JobInfo& job, ProcessedPCoeffSum* countConnectedSumBuf) {
	Monotonic<Variables> top = downLinkStructure.mbfs[job.getTop()];

	BooleanFunction<Variables> graphsBuf[factorial(Variables)];

	for(const NodeIndex* cur = job.begin(); cur != job.end(); cur++) {
		Monotonic<Variables> bot = downLinkStructure.mbfs[*cur];
		countConnectedSumBuf[job.indexOf(cur)] = processPCoeffSum<Variables>(top, bot, graphsBuf);
	}
}

template<unsigned int Variables>
void processBetasCPU_MultiThread(const FlatMBFStructure<Variables>& downLinkStructure, const JobInfo& job, ProcessedPCoeffSum* countConnectedSumBuf, ThreadPool& threadPool) {
	Monotonic<Variables> top = downLinkStructure.mbfs[job.getTop()];

	std::atomic<const NodeIndex*> i = job.begin();
	threadPool.doInParallel([&](){
		BooleanFunction<Variables> graphsBuf[factorial(Variables)];
		while(true) {
			const NodeIndex* claimedNodeIndex = i.fetch_add(1);
			if(claimedNodeIndex >= job.end()) break;

			Monotonic<Variables> bot = downLinkStructure.mbfs[*claimedNodeIndex];

			countConnectedSumBuf[job.indexOf(claimedNodeIndex)] = processPCoeffSum<Variables>(top, bot, graphsBuf);
		}
	});
}

template<unsigned int Variables, size_t BatchSize>
void buildJobBatch(const FlatMBFStructure<Variables>& allMBFData, NodeIndex* tops, size_t numberOfTops, JobBatch<Variables, BatchSize>& jobBatch, SwapperLayers<Variables, BitSet<BatchSize>>& swapper) {
	jobBatch.initialize(allMBFData, tops, numberOfTops);

#ifdef PCOEFF_DEDUPLICATE
	computeBuffersDeduplicate(allMBFData, jobBatch, swapper);
#else
	computeBuffers(allMBFData, jobBatch, swapper);
#endif
}

template<unsigned int Variables, size_t BatchSize>
void computeBatchBetaSums(const FlatMBFStructure<Variables>& allMBFData, JobBatch<Variables, BatchSize>& jobBatch, ProcessedPCoeffSum* pcoeffSumBuf, BetaSum* results) {
	
#ifdef PCOEFF_MULTITHREAD
	ThreadPool threadPool;
#endif

	for(size_t i = 0; i < jobBatch.jobCount; i++) {
		const JobInfo& curJob = jobBatch.jobs[i];
		#ifdef PCOEFF_MULTITHREAD
		processBetasCPU_MultiThread(allMBFData, curJob, pcoeffSumBuf, threadPool);
		#else
		processBetasCPU_SingleThread(allMBFData, curJob, pcoeffSumBuf);
		#endif

		results[i] = produceBetaResult(allMBFData, curJob, pcoeffSumBuf);
	}
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

template<unsigned int Variables, size_t BatchSize>
u192 flatDPlus2() {
	FlatMBFStructure<Variables> allMBFData = readFlatMBFStructure<Variables>();
	SwapperLayers<Variables, BitSet<BatchSize>> swapper;
	JobBatch<Variables, BatchSize> jobBatch;

	for(size_t i = 0; i < BatchSize; i++) {
		jobBatch.jobs[i].bufStart = static_cast<NodeIndex*>(malloc(sizeof(NodeIndex) * MAX_BUFSIZE(Variables)));
	}

	std::vector<BetaResult> betaResults;
	betaResults.reserve(mbfCounts[Variables]);

	ProcessedPCoeffSum* pcoeffSumBuf = new ProcessedPCoeffSum[mbfCounts[Variables]];

	for(NodeIndex curIndex = 0; curIndex < mbfCounts[Variables]; curIndex += BatchSize) {
		std::cout << '.' << std::flush;

		NodeOffset numberToProcess = static_cast<NodeOffset>(std::min(mbfCounts[Variables] - curIndex, BatchSize));
		
		NodeIndex indexBuf[BatchSize];
		for(size_t i = 0; i < numberToProcess; i++) {
			indexBuf[i] = curIndex + i;
		}

		buildJobBatch(allMBFData, indexBuf, numberToProcess, jobBatch, swapper);

		BetaSum resultingBetaSums[BatchSize];
		computeBatchBetaSums(allMBFData, jobBatch, pcoeffSumBuf, resultingBetaSums);

		for(size_t i = 0; i < numberToProcess; i++) {
			BetaResult newResult;
			newResult.betaSum = resultingBetaSums[i];
			newResult.topIndex = jobBatch.jobs[i].getTop();
			betaResults.push_back(newResult);
		}
	}
	
	BetaResultCollector collector(Variables);
	collector.addBetaResults(betaResults);

	u192 dedekindNumber = computeDedekindNumberFromBetaSums(allMBFData, collector.getResultingSums());
	std::cout << "D(" << (Variables + 2) << ") = " << dedekindNumber << std::endl;

	for(size_t i = 0; i < BatchSize; i++) {
		free(jobBatch.jobs[i].bufStart);
	}

	return dedekindNumber;
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

