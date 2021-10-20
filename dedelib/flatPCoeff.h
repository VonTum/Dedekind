#pragma once

#include <atomic>

#include "knownData.h"
#include "flatMBFStructure.h"
#include "swapperLayers.h"
#include "bitSet.h"
#include "u192.h"
#include "connectGraph.h"
#include "threadPool.h"

#define PCOEFF_MULTITHREAD
#define PCOEFF_DEDUPLICATE


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
	const FlatNode* firstNode = downLinkStructure.allNodes + FlatMBFStructure<Variables>::cachedOffsets[curLayer];
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



struct NodeIndexBuffer {
	NodeIndex* bufStart;
	NodeIndex* bufEnd;
	
	NodeIndexBuffer() : bufStart(nullptr), bufEnd(nullptr) {}
	NodeIndexBuffer(size_t size) : bufStart((NodeIndex*) malloc(sizeof(NodeIndex) * size)), bufEnd(bufStart) {}
	~NodeIndexBuffer() {if(bufStart) free(bufStart);}
	NodeIndexBuffer(const NodeIndexBuffer&) = delete;
	NodeIndexBuffer& operator=(const NodeIndexBuffer&) = delete;
	NodeIndexBuffer(NodeIndexBuffer&& other) noexcept : 
		bufStart(other.bufStart), bufEnd(other.bufEnd) {
		other.bufStart = nullptr;
		other.bufEnd = nullptr;
	}
	NodeIndexBuffer& operator=(NodeIndexBuffer&& other) noexcept {
		std::swap(this->bufStart, other.bufStart);
		std::swap(this->bufEnd, other.bufEnd);
		return *this;
	}

	void add(NodeIndex idx) {
		*bufEnd = idx;
		bufEnd++;
	}
	void clear() {
		bufEnd = bufStart;
	}
	size_t size() const {
		return bufEnd - bufStart;
	}
};

template<unsigned int Variables>
struct JobInfo {
	NodeIndex top;
	NodeIndex topDual;
	int topLayer;
	NodeIndexBuffer indexBuffer;

	JobInfo() : indexBuffer(mbfCounts[Variables]) {}

	void initialize(NodeIndex top, NodeIndex topDual, int topLayer) {
		this->top = top;
		this->topDual = topDual;
		this->topLayer = topLayer;
		indexBuffer.clear();
	}
};

template<unsigned int Variables, size_t BatchSize>
struct JobBatch {
	JobInfo<Variables> jobs[BatchSize];
	size_t jobCount;

	void initialize(const FlatMBFStructure<Variables>& downLinkStructure, NodeIndex* tops, size_t jobCount = BatchSize) {
		assert(jobCount <= BatchSize);
		for(size_t i = 0; i < jobCount; i++) {
			NodeIndex top = tops[i];
			JobInfo<Variables>& job = jobs[i];

			NodeIndex topDual = downLinkStructure.allNodes[top].dual;
			int topLayer = FlatMBFStructure<Variables>::getLayer(top);

			job.initialize(top, topDual, topLayer);
		}
		this->jobCount = jobCount;
	}

	void initializeFromTo(const FlatMBFStructure<Variables>& downLinkStructure, NodeIndex topsFrom, size_t jobCount = BatchSize) {
		assert(jobCount <= BatchSize);
		NodeIndex indexBuf[BatchSize];
		for(size_t i = 0; i < jobCount; i++) {
			indexBuf[i] = topsFrom + i;
		}
		this->initialize(downLinkStructure, indexBuf, jobCount);
	}
	
	int getHighestLayer() const {
		int highestLayer = jobs[0].topLayer;
		for(size_t i = 1; i < jobCount; i++){
			const JobInfo<Variables>& j = jobs[i];
			if(j.topLayer > highestLayer) highestLayer = j.topLayer;
		}
		return highestLayer;
	}
	
	int getLowestLayer() const {
		int lowestLayer = jobs[0].topLayer;
		for(size_t i = 1; i < jobCount; i++){
			const JobInfo<Variables>& j = jobs[i];
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
				JobInfo<Variables>& job = jobBatch.jobs[jobIdx];
				job.indexBuffer.add(curNodeIndex);
			}
		}
	}
}

template<unsigned int Variables, size_t BatchSize>
void addSourceElementsToJobsWithDeDuplication(const SwapperLayers<Variables, BitSet<BatchSize>>& swapper, JobBatch<Variables, BatchSize>& jobBatch) {
	NodeIndex firstNodeInLayer = FlatMBFStructure<Variables>::cachedOffsets[swapper.getCurrentLayerDownward()];
	for(NodeOffset i = 0; i < NodeOffset(swapper.getSourceLayerSize()); i++) {
		BitSet<BatchSize> includedBits = swapper.source(i);
		if(includedBits.isEmpty()) continue;

		NodeIndex curNodeIndex = firstNodeInLayer + i;

		for(size_t jobIdx = 0; jobIdx < BatchSize; jobIdx++) {
			if(includedBits.get(jobIdx)) {
				JobInfo<Variables>& job = jobBatch.jobs[jobIdx];
				if(job.topDual > curNodeIndex) {// Arbitrary Equivalence Class Ordering for Deduplication
					job.indexBuffer.add(curNodeIndex);
				}
			}
		}
	}
}


template<unsigned int Variables, size_t BatchSize>
void initializeSwapper(SwapperLayers<Variables, BitSet<BatchSize>>& swapper, const JobBatch<Variables, BatchSize>& jobBatch) {
	int curSwapperLayer = swapper.getCurrentLayerDownward();
	NodeIndex layerStart = FlatMBFStructure<Variables>::cachedOffsets[curSwapperLayer];
	for(size_t i = 0; i < jobBatch.jobCount; i++) {
		if(jobBatch.jobs[i].topLayer == curSwapperLayer) {
			NodeOffset indexInLayer = jobBatch.jobs[i].top - layerStart;
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

template<unsigned int Variables>
ProcessedPCoeffSum processPCoeffSum(Monotonic<Variables> top, Monotonic<Variables> bot, BooleanFunction<Variables> graphsBuf[factorial(Variables)]) {
	BooleanFunction<Variables>* graphsBufEnd = listPermutationsBelow<Variables>(top, bot, graphsBuf);
	ProcessedPCoeffSum result;
	result.pcoeffCount = graphsBufEnd - graphsBuf;
	result.pcoeffSum = computePCoeffSum(graphsBuf, graphsBufEnd);
	return result;
}

template<unsigned int Variables>
ProcessedPCoeffSum processOneBeta(const FlatMBFStructure<Variables>& downLinkStructure, NodeIndex topIdx, NodeIndex botIdx) {
	Monotonic<Variables> top = downLinkStructure.mbfs[topIdx];
	Monotonic<Variables> bot = downLinkStructure.mbfs[botIdx];

	BooleanFunction<Variables> graphsBuf[factorial(Variables)];
	return processPCoeffSum<Variables>(top, bot, graphsBuf);
}

template<unsigned int Variables>
void processBetasCPU_SingleThread(const FlatMBFStructure<Variables>& downLinkStructure, 
NodeIndex topIdx, const NodeIndex* idxBuf, const NodeIndex* bufEnd, ProcessedPCoeffSum* countConnectedSumBuf) {
	Monotonic<Variables> top = downLinkStructure.mbfs[topIdx];

	BooleanFunction<Variables> graphsBuf[factorial(Variables)];

	for(; idxBuf != bufEnd; idxBuf++) {
		Monotonic<Variables> bot = downLinkStructure.mbfs[*idxBuf];
		*countConnectedSumBuf++ = processPCoeffSum<Variables>(top, bot, graphsBuf);
	}
}

template<unsigned int Variables>
void processBetasCPU_MultiThread(const FlatMBFStructure<Variables>& downLinkStructure, 
NodeIndex topIdx, const NodeIndex* idxBuf, const NodeIndex* bufEnd, ProcessedPCoeffSum* countConnectedSumBuf, ThreadPool& threadPool) {
	Monotonic<Variables> top = downLinkStructure.mbfs[topIdx];

	size_t idxBufSize = bufEnd - idxBuf;
	std::atomic<size_t> i = 0;
	threadPool.doInParallel([&](){
		BooleanFunction<Variables> graphsBuf[factorial(Variables)];
		while(true) {
			size_t claimedI = i.fetch_add(1);
			if(claimedI >= idxBufSize) break;

			Monotonic<Variables> bot = downLinkStructure.mbfs[idxBuf[claimedI]];

			countConnectedSumBuf[claimedI] = processPCoeffSum<Variables>(top, bot, graphsBuf);
		}
	});
}

struct BetaSum {
	u128 betaSum;
	
	// validation data
	uint64_t countedIntervalSizeDown;
};

inline BetaSum operator+(BetaSum a, BetaSum b) {
	return BetaSum{a.betaSum + b.betaSum, a.countedIntervalSizeDown + b.countedIntervalSizeDown};
}
inline BetaSum& operator+=(BetaSum& a, BetaSum b) {
	a.betaSum += b.betaSum;
	a.countedIntervalSizeDown += b.countedIntervalSizeDown;
	return a;
}

// does the necessary math with annotated number of bits, no overflows possible for D(9). 
template<unsigned int Variables>
BetaSum produceBetaTerm(ClassInfo info, ProcessedPCoeffSum processedPCoeff) {
	constexpr unsigned int VAR_FACTORIAL = factorial(Variables);

	// the multiply is max log2(2^35 * 5040 * 5040) = 59.5984160368 bits long, fits in 64 bits
	// Compiler optimizes the divide by compile-time constant VAR_FACTORIAL to an imul, much faster!
	uint64_t deduplicatedTotalPCoeffSum = (processedPCoeff.pcoeffSum * info.classSize) / VAR_FACTORIAL; 
	u128 betaTerm = umul128(deduplicatedTotalPCoeffSum, info.intervalSizeDown); // no overflow data loss 64x64->128 bits

	// validation data
	uint64_t deduplicatedCountedPermutes = (processedPCoeff.pcoeffCount * info.classSize) / VAR_FACTORIAL;

	return BetaSum{betaTerm, deduplicatedCountedPermutes};
}

template<unsigned int Variables>
BetaSum sumOverBetas(const FlatMBFStructure<Variables>& downLinkStructure, 
const NodeIndex* idxBuf, const NodeIndex* bufEnd, const ProcessedPCoeffSum* countConnectedSumBuf) {
	BetaSum total = BetaSum{0,0};

	for(; idxBuf != bufEnd; idxBuf++) {
		ClassInfo info = downLinkStructure.allClassInfos[*idxBuf];
		total += produceBetaTerm<Variables>(info, *countConnectedSumBuf);
		countConnectedSumBuf++;
	}
	return total;
}

#define VALIDATE(topIdx, condition) if(!(condition)) throw "INVALID";

template<unsigned int Variables, size_t BatchSize>
void computeBatchBetaSums(const FlatMBFStructure<Variables>& allMBFData, JobBatch<Variables, BatchSize>& jobBatch, SwapperLayers<Variables, BitSet<BatchSize>>& swapper, ProcessedPCoeffSum* pcoeffSumBuf, BetaSum* results) {
	
#ifdef PCOEFF_MULTITHREAD
	ThreadPool threadPool;
#endif

#ifdef PCOEFF_DEDUPLICATE
	computeBuffersDeduplicate(allMBFData, jobBatch, swapper);
#else
	computeBuffers(allMBFData, jobBatch, swapper);
#endif

	for(size_t i = 0; i < jobBatch.jobCount; i++) {
		const JobInfo<Variables>& curJob = jobBatch.jobs[i];
		#ifdef PCOEFF_MULTITHREAD
		processBetasCPU_MultiThread(allMBFData, curJob.top, curJob.indexBuffer.bufStart, curJob.indexBuffer.bufEnd, pcoeffSumBuf, threadPool);
		#else
		processBetasCPU_SingleThread(allMBFData, curJob.top, curJob.indexBuffer.bufStart, curJob.indexBuffer.bufEnd, pcoeffSumBuf);
		#endif
		BetaSum jobSum = sumOverBetas(allMBFData, curJob.indexBuffer.bufStart, curJob.indexBuffer.bufEnd, pcoeffSumBuf);

#ifdef PCOEFF_DEDUPLICATE
		ProcessedPCoeffSum nonDuplicateTopDual = processOneBeta(allMBFData, curJob.top, curJob.topDual);
		
		ClassInfo info = allMBFData.allClassInfos[curJob.topDual];

		BetaSum nonDuplicateTopDualResult = produceBetaTerm<Variables>(info, nonDuplicateTopDual);

		results[i] = jobSum + jobSum + nonDuplicateTopDualResult;
#else
		results[i] = jobSum;
#endif
	}
}

template<unsigned int Variables, size_t BatchSize>
u192 flatDPlus2() {
	FlatMBFStructure<Variables> allMBFData = readFlatMBFStructure<Variables>();
	SwapperLayers<Variables, BitSet<BatchSize>> swapper;
	JobBatch<Variables, BatchSize> jobBatch;

	u192 total = 0;
	NodeIndex curIndex = 0;

	ProcessedPCoeffSum* pcoeffSumBuf = new ProcessedPCoeffSum[mbfCounts[Variables]];

	while(curIndex < mbfCounts[Variables]) {
		std::cout << '.' << std::flush;

		NodeOffset numberToProcess = static_cast<NodeOffset>(std::min(mbfCounts[Variables] - curIndex, BatchSize));

		jobBatch.initializeFromTo(allMBFData, curIndex, numberToProcess);

		BetaSum resultingBetaSums[BatchSize];
		computeBatchBetaSums(allMBFData, jobBatch, swapper, pcoeffSumBuf, resultingBetaSums);

		for(size_t i = 0; i < numberToProcess; i++) {
			NodeIndex curJobTop = jobBatch.jobs[i].top;
			ClassInfo topInfo = allMBFData.allClassInfos[curJobTop];
			
			// invalid for DEDUPLICATION
			// VALIDATE(curJobTop, jobSum.countedIntervalSizeDown == topInfo.intervalSizeDown);

			uint64_t topIntervalSizeUp = allMBFData.allClassInfos[allMBFData.allNodes[curJobTop].dual].intervalSizeDown;
			uint64_t topFactor = topIntervalSizeUp * topInfo.classSize; // max log2(2414682040998*5040) = 53.4341783883
			total += umul192(resultingBetaSums[i].betaSum, topFactor);
		}

		curIndex += numberToProcess;
	}
	
	std::cout << "D(" << (Variables + 2) << ") = " << total << std::endl;
	return total;
}



