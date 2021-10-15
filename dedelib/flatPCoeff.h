#pragma once

#include "knownData.h"
#include "flatMBFStructure.h"
#include "swapperLayers.h"
#include "bitSet.h"
#include "u192.h"
#include "connectGraph.h"
#include "threadPool.h"

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
	NodeIndexBuffer indexBuffer;

	JobInfo() : indexBuffer(mbfCounts[Variables]) {}

	void initialize(NodeIndex top) {
		this->top = top;
		indexBuffer.clear();
	}
};

template<unsigned int Variables, size_t BatchSize>
struct JobBatch {
	JobInfo<Variables> jobs[BatchSize];

	void initialize(NodeIndex* tops, size_t topCount = BatchSize) {
		for(size_t i = 0; i < topCount; i++) {
			jobs[i].initialize(tops[i]);
			jobs[i].indexBuffer.add(tops[i]);
		}
	}
};


template<unsigned int Variables, size_t BatchSize>
void addSourceElementsToJobs(const SwapperLayers<Variables, BitSet<BatchSize>>& swapper, JobBatch<Variables, BatchSize>& jobBatch) {
	NodeIndex firstNodeInLayer = FlatMBFStructure<Variables>::cachedOffsets[swapper.getCurrentLayerDownward()];
	for(NodeOffset i = 0; i < NodeOffset(swapper.getSourceLayerSize()); i++) {
		BitSet<BatchSize> includedBits = swapper.source(i);
		if(includedBits.isEmpty()) continue;

		NodeIndex curNodeIndex = firstNodeInLayer + i;

		for(size_t job = 0; job < BatchSize; job++) {
			if(includedBits.get(job)) {
				jobBatch.jobs[job].indexBuffer.add(curNodeIndex);
			}
		}
	}
}


template<unsigned int Variables, size_t BatchSize>
void initializeSwapper(SwapperLayers<Variables, BitSet<BatchSize>>& swapper, NodeIndex* tops, size_t topCount = BatchSize) {
	NodeIndex layerStart = FlatMBFStructure<Variables>::cachedOffsets[swapper.getCurrentLayerDownward()];
	NodeIndex layerEnd = FlatMBFStructure<Variables>::cachedOffsets[swapper.getCurrentLayerDownward()+1];
	for(size_t i = 0; i < topCount; i++) {
		if(tops[i] >= layerStart && tops[i] < layerEnd) {
			swapper.source(tops[i] - layerStart).set(i);
		}
	}
}

template<unsigned int Variables>
int getHighestLayer(NodeIndex* tops, size_t topCount) {
	for(int startingLayer = (1 << Variables); startingLayer >= 0; startingLayer--) {
		NodeIndex layerStart = FlatMBFStructure<Variables>::cachedOffsets[startingLayer];
		NodeIndex layerEnd = FlatMBFStructure<Variables>::cachedOffsets[startingLayer+1];
		for(size_t i = 0; i < topCount; i++) {
			if(tops[i] >= layerStart && tops[i] < layerEnd) return startingLayer;
		}
	}
	assert(false);
	#ifdef __GNUC__
	__builtin_unreachable();
	#endif
}

template<unsigned int Variables, size_t BatchSize>
void computeBuffers(const FlatMBFStructure<Variables>& downLinkStructure, JobBatch<Variables, BatchSize>& jobBatch, SwapperLayers<Variables, BitSet<BatchSize>>& swapper, NodeIndex* tops, size_t topCount = BatchSize) {
	int currentLayer = getHighestLayer<Variables>(tops, topCount);

	jobBatch.initialize(tops, topCount);
	swapper.resetDownward(currentLayer);

	for(; currentLayer > 0; currentLayer--) {
		initializeSwapper(swapper, tops, topCount);

		followNextLayerLinks(downLinkStructure, swapper);

		addSourceElementsToJobs(swapper, jobBatch);
	}

	assert(!swapper.canPushNext());
}
template<unsigned int Variables, size_t BatchSize>
void computeBuffersFromTop(const FlatMBFStructure<Variables>& downLinkStructure, JobBatch<Variables, BatchSize>& jobBatch, SwapperLayers<Variables, BitSet<BatchSize>>& swapper, NodeIndex firstTop, size_t topCount = BatchSize) {
	NodeIndex tops[BatchSize];
	for(NodeOffset i = 0; i < topCount; i++) {
		tops[i] = firstTop + i;
	}
	computeBuffers(downLinkStructure, jobBatch, swapper, tops, topCount);
}

template<unsigned int Variables>
BooleanFunction<Variables>* listPermutationsBelow(const Monotonic<Variables>& top, const Monotonic<Variables>& botToPermute, BooleanFunction<Variables>* result) {
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
void processBetasCPU_SingleThread(const FlatMBFStructure<Variables>& downLinkStructure, 
NodeIndex topIdx, const NodeIndex* idxBuf, const NodeIndex* bufEnd, ProcessedPCoeffSum* countConnectedSumBuf) {
	Monotonic<Variables> top = downLinkStructure.mbfs[topIdx];

	BooleanFunction<Variables> graphsBuf[factorial(Variables)];

	for(; idxBuf != bufEnd; idxBuf++) {
		Monotonic<Variables> bot = downLinkStructure.mbfs[*idxBuf];

		BooleanFunction<Variables>* graphsBufEnd = listPermutationsBelow(top, bot, graphsBuf);
		ProcessedPCoeffSum newElem;
		newElem.pcoeffCount = graphsBufEnd - graphsBuf;
		newElem.pcoeffSum = computePCoeffSum(graphsBuf, graphsBufEnd);
		*countConnectedSumBuf = newElem;
		countConnectedSumBuf++;
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
			BooleanFunction<Variables>* graphsBufEnd = listPermutationsBelow(top, bot, graphsBuf);
			ProcessedPCoeffSum newElem;
			newElem.pcoeffCount = graphsBufEnd - graphsBuf;
			newElem.pcoeffSum = computePCoeffSum(graphsBuf, graphsBufEnd);
			countConnectedSumBuf[claimedI] = newElem;
		}
	});
}

struct BetaSum {
	u128 betaSum;
	uint64_t countedIntervalSizeDown;
};

template<unsigned int Variables>
BetaSum sumOverBetas(const FlatMBFStructure<Variables>& downLinkStructure, 
const NodeIndex* idxBuf, const NodeIndex* bufEnd, const ProcessedPCoeffSum* countConnectedSumBuf) {
	u128 sum = 0;
	uint64_t countedIntervalSizeDown = 0;
	constexpr unsigned int VAR_FACTORIAL = factorial(Variables);
	for(; idxBuf != bufEnd; idxBuf++) {
		ClassInfo info = downLinkStructure.allClassInfos[*idxBuf];
		uint64_t pcoeffSum = countConnectedSumBuf->pcoeffSum;
		uint64_t countedPermutes = countConnectedSumBuf->pcoeffCount;
		// the multiply is max log2(2^35 * 5040 * 5040) = 59.5984160368 bits long, fits in 64 bits
		uint64_t deduplicatedTotalPCoeffSum = (pcoeffSum * info.classSize) / VAR_FACTORIAL; // Compiler optimizes the divide by constant to an imul, much faster!
		countedIntervalSizeDown += (countedPermutes * info.classSize) / VAR_FACTORIAL;
		sum += umul128(deduplicatedTotalPCoeffSum, info.intervalSizeDown);
		countConnectedSumBuf++;
	}
	return BetaSum{sum, countedIntervalSizeDown};
}

#define VALIDATE(topIdx, condition) if(!(condition)) throw "INVALID";

#define PCOEFF_MULTITHREAD

template<unsigned int Variables, size_t BatchSize>
u192 flatDPlus2() {
	FlatMBFStructure<Variables> allMBFData = readFlatMBFStructure<Variables>();
	SwapperLayers<Variables, BitSet<BatchSize>> swapper;
	JobBatch<Variables, BatchSize> jobBatch;

	u192 total = 0;
	NodeIndex curIndex = 0;

	ProcessedPCoeffSum* pcoeffSumBuf = new ProcessedPCoeffSum[mbfCounts[Variables]];

#ifdef PCOEFF_MULTITHREAD
	ThreadPool threadPool;
#endif

	while(curIndex < mbfCounts[Variables]) {
		std::cout << '.' << std::flush;

		NodeOffset numberToProcess = static_cast<NodeOffset>(std::min(mbfCounts[Variables] - curIndex, BatchSize));

		computeBuffersFromTop(allMBFData, jobBatch, swapper, curIndex, numberToProcess);

		for(NodeOffset i = 0; i < numberToProcess; i++) {
			const JobInfo<Variables>& curJob = jobBatch.jobs[i];
			#ifdef PCOEFF_MULTITHREAD
			processBetasCPU_MultiThread(allMBFData, curJob.top, curJob.indexBuffer.bufStart, curJob.indexBuffer.bufEnd, pcoeffSumBuf, threadPool);
			#else
			processBetasCPU_SingleThread(allMBFData, curJob.top, curJob.indexBuffer.bufStart, curJob.indexBuffer.bufEnd, pcoeffSumBuf);
			#endif
			BetaSum jobSum = sumOverBetas(allMBFData, curJob.indexBuffer.bufStart, curJob.indexBuffer.bufEnd, pcoeffSumBuf);

			ClassInfo topInfo = allMBFData.allClassInfos[curJob.top];
			uint64_t topIntervalSizeUp = allMBFData.allClassInfos[allMBFData.allNodes[curJob.top].dual].intervalSizeDown;
			
			VALIDATE(curJob.top, jobSum.countedIntervalSizeDown == topInfo.intervalSizeDown);
			uint64_t topFactor = topIntervalSizeUp * topInfo.classSize; // max log2(2414682040998*5040)
			total += umul192(jobSum.betaSum, topFactor);
		}

		curIndex += numberToProcess;
	}
	
	std::cout << "D(" << (Variables + 2) << ") = " << total << std::endl;
	return total;
}



